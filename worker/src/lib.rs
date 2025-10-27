use serde::{Deserialize, Serialize};
use std::fmt;
use worker::*;

use aes::Aes256;
use cbc::{Encryptor, Decryptor};
use cbc::cipher::{BlockEncryptMut, BlockDecryptMut, KeyIvInit};
use getrandom::getrandom;
use base64::{Engine as _, engine::general_purpose};

type Aes256CbcEnc = Encryptor<Aes256>;
type Aes256CbcDec = Decryptor<Aes256>;




#[derive(Clone, Serialize, Deserialize, Debug)]
struct Message {
    id: Option<u32>,
    sender: String,
    sms: String,
    ts: String,
}

impl fmt::Display for Message {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "Sender: {} ({}); Message: {};",
            self.sender, self.ts, self.sms
        )
    }
}

#[derive(Serialize)]
struct ResendEmailPayload {
    from: String,
    to: Vec<String>,
    subject: String,
    html: String,
}


#[event(fetch)]
async fn fetch(req: Request, env: Env, _ctx: Context) -> Result<Response> {
    let api_key = env.secret("AUTH_KEY")?.to_string();
    let authorization = req.headers().get("Authorization")?;
    if authorization == None {
        return Response::error("Unauthorized: Invalid JSON", 401);
    }
    let authorization = authorization.unwrap();
    let auth: Vec<&str> = authorization.split(" ").collect();
    let scheme = auth[0];
    let encoded = auth[1];
    if encoded == "" || scheme != "Cisab" {
        return Response::error("Bad Request: Invalid JSON", 400);
    }

    if encoded.chars().rev().collect::<String>() != api_key {
        return Response::error("Unauthorized: Invalid JSON", 401);
    }

    Router::new()
        .get("/health", |_, _| Response::ok("yo"))
        .get_async("/get", |_, ctx| async move { handle_get_all(ctx).await })
        .get_async(
            "/get/:id",
            |_, ctx| async move { handle_get_by_id(ctx).await },
        )
        .post_async("/set", |req, ctx| async move {
            handle_set_message(req, ctx).await
        })
        .run(req, env)
        .await
}

async fn handle_get_all(ctx: RouteContext<()>) -> Result<Response> {
    let d1 = ctx.env.d1("DB")?;
    let statement = d1.prepare("SELECT * FROM messages ORDER BY id DESC");

    let encryption_key = ctx.env.var("ENCRYPTION_KEY")?.to_string();

    match statement.all().await {
        Ok(results) => {
            console_debug!("Raw D1 Result: {:?}", results);
            match results.results::<Message>() {
                Ok(mut messages) => {
                    // Decrypt each message
                    for message in messages.iter_mut() {
                        match decrypt_message(&message.sms, &encryption_key) {
                            Ok(decrypted) => message.sms = decrypted,
                            Err(e) => {
                                console_error!(
                                    "Failed to decrypt message {:?}: {:?}",
                                    message.id,
                                    e
                                );
                                // message.sms = "[Decryption failed]".to_string();
                            }
                        }
                    }
                    Response::from_json(&messages)
                }
                Err(e) => {
                    console_error!("Failed to deserialize messages: {:?}", e);
                    Response::error("Internal Server Error: Data format issue", 500)
                }
            }
        }
        Err(e) => {
            console_error!("D1 query failed for all messages: {:?}", e);
            Response::error("Internal Server Error: Database query failed", 500)
        }
    }
}

async fn handle_get_by_id(ctx: RouteContext<()>) -> Result<Response> {
    let id = match ctx.param("id") {
        Some(s) => s.to_string(),
        None => return Response::error("Bad Request: ID missing", 400),
    };
    let encryption_key = ctx.env.var("ENCRYPTION_KEY")?.to_string();

    let d1 = ctx.env.d1("DB")?;
    let statement = d1.prepare("SELECT * FROM messages WHERE id = ?1");

    let query = statement.bind(&[id.into()])?;

    match query.first::<Message>(None).await? {
        Some(mut message) => {
            message.sms = decrypt_message(&message.sms, &encryption_key).map_err(|e| {
                console_error!("Encryption failed: {:?}", e);
                worker::Error::from("Encryption failed")
            })?;
            Response::from_json(&message)
        }
        None => Response::error("Message Not Found", 404),
    }
}

async fn handle_set_message(mut req: Request, ctx: RouteContext<()>) -> Result<Response> {
    let mut message_body: Message = match req.json().await {
        Ok(b) => b,
        Err(e) => {
            console_error!("Failed to parse request: {:?}", e);
            return Response::error("Bad Request: Invalid JSON", 400);
        }
    };

    console_debug!("Received: {:?}", message_body);

    if message_body.sms.trim().is_empty() {
        return Response::error("Bad Request: SMS content cannot be empty", 400);
    }

    let encryption_key = ctx.env.var("ENCRYPTION_KEY")?.to_string();

    message_body.sms = encrypt_message(&message_body.sms, &encryption_key).map_err(|e| {
        console_error!("Encryption failed: {:?}", e);
        worker::Error::from("Encryption failed")
    })?;

    let d1 = ctx.env.d1("DB")?;

    let statement = d1.prepare("INSERT INTO messages (sender, sms, ts) VALUES (?1, ?2, ?3)");

    let query = statement.bind(&[
        message_body.sender.clone().into(),
        message_body.sms.clone().into(),
        message_body.ts.clone().into(),
    ])?;

    match query.run().await {
        Ok(result) if result.success() => {
            console_log!("Message successfully stored.");

            if let Err(e) = send_mail(message_body, &ctx).await {
                console_error!("Failed to send email: {:?}", e);
            }

            Response::empty().map(|resp| resp.with_status(201))
        }
        Ok(_) => {
            console_error!("D1 run failed");
            Response::error("D1 operation failed", 500)
        }
        Err(e) => {
            console_error!("D1 run failed with error: {:?}", e);
            Response::error("Internal Server Error: Database insertion failed", 500)
        }
    }
}

async fn send_mail(msg: Message, ctx: &RouteContext<()>) -> Result<()> {
    // Get environment variables
    let api_key = ctx.env.secret("RESEND_API_KEY")?.to_string(); //
    let from_email = ctx.env.var("FROM_EMAIL")?.to_string(); //
    let to_email = ctx.env.var("TO_EMAIL")?.to_string();
    console_log!("{} {} {}", api_key, from_email, to_email);
    // Create HTML email body
    let html_body = format!(
        r#"
        <html>
            <body>
                <h2>New SMS from Lghnay</h2>
                <p><strong>Sender:</strong> {}</p>
                <p><strong>Time:</strong> {}</p>
                <p><strong>Message:</strong></p>
                <p>{}</p>
            </body>
        </html>
        "#,
        msg.sender, msg.ts, msg.sms
    );

    // Prepare the email payload
    let payload = ResendEmailPayload {
        from: from_email,
        to: vec![to_email],
        subject: "New SMS from Lghnay".to_string(),
        html: html_body,
    };

    // Create headers
    let headers = Headers::new();
    headers.set("Authorization", &format!("Bearer {}", api_key))?;
    headers.set("Content-Type", "application/json")?;

    // Create the request
    let body = serde_json::to_string(&payload)
        .map_err(|e| Error::RustError(format!("Serialization error: {}", e)))?;

    let req = Request::new_with_init(
        "https://api.resend.com/emails",
        RequestInit::new()
            .with_method(Method::Post)
            .with_headers(headers)
            .with_body(Some(body.into())),
    )?;

    // Send the request
    let mut resp = Fetch::Request(req).send().await?;

    // Check response status
    let status = resp.status_code();
    if status < 200 || status >= 300 {
        let error_text = resp.text().await?;
        console_error!("Resend API error ({}): {}", resp.status_code(), error_text);
        return Err(Error::RustError(format!(
            "Resend API error ({}): {}",
            resp.status_code(),
            error_text
        )));
    }

    // Parse response
    let resend_resp: Message = resp.json().await?;
    console_log!("Email sent successfully! ID: {}", resend_resp.to_string());

    Ok(())
}



pub fn encrypt_message(message: &str, encryption_key: &str) -> std::result::Result<String, Box<dyn std::error::Error>> {
    let key = encryption_key.as_bytes();
    
    let mut iv = [0u8; 16];
    getrandom(&mut iv).map_err(|e| format!("Random generation error: {:?}", e))?;    
    let mut buffer = message.as_bytes().to_vec();
    let padding = 16 - (buffer.len() % 16);
    buffer.extend(vec![padding as u8; padding]);
    
    let cipher = Aes256CbcEnc::new(key.into(), &iv.into());
    let ciphertext = cipher.encrypt_padded_b2b_mut::<cbc::cipher::block_padding::Pkcs7>(
        message.as_bytes(),
        &mut buffer
    ).map_err(|e| format!("Encryption error: {:?}", e))?;
    
    let mut combined = Vec::with_capacity(16 + ciphertext.len());
    combined.extend_from_slice(&iv);
    combined.extend_from_slice(ciphertext);
    
    let encoded = general_purpose::STANDARD.encode(&combined);
    
    Ok(encoded)
}



pub fn decrypt_message(encrypted_message: &str, encryption_key: &str) -> std::result::Result<String, Box<dyn std::error::Error>> {
    let decoded = general_purpose::STANDARD.decode(encrypted_message)?;
    
    if decoded.len() < 16 {
        return Err("Invalid ciphertext length".into());
    }
    
    let (iv, ciphertext) = decoded.split_at(16);
    let mut buffer = ciphertext.to_vec();
    
    let key = encryption_key.as_bytes();
    let cipher = Aes256CbcDec::new(key.into(), iv.into());
    
    let decrypted = cipher.decrypt_padded_mut::<cbc::cipher::block_padding::Pkcs7>(&mut buffer)
        .map_err(|e| format!("Decryption error: {:?}", e))?;
    
    let plaintext = String::from_utf8(decrypted.to_vec())?;
    
    Ok(plaintext)
}