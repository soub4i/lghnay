package internal

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
)

func (r *Repository) GetMessages() *[]Message {
	logger := GetLogger()
	store := r.ctx.Value("store").(map[string]string)
	u := fmt.Sprintf("%s/get", store["URL"])
	req, err := http.NewRequest(http.MethodGet, u, nil)
	if err != nil {
		logger.Printf("error building request %v", err)
		return nil
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", fmt.Sprintf("Cisab %s", store["TOKEN"]))
	res, err := r.http.Do(req)
	if err != nil {
		logger.Printf("error fetching data from server [%s] ; %v", u, err)
		return nil
	}
	if res.StatusCode != 200 {
		logger.Printf("Rxpected 200, got [%d]", res.StatusCode)
		return nil
	}
	var msg []Message
	body, err := io.ReadAll(res.Body)
	if err != nil {
		logger.Printf("error reading body; %v", err)
		return nil
	}
	err = json.Unmarshal(body, &msg)
	if err != nil {
		logger.Printf("error unmarshell body; %v", err)
		return nil
	}

	for i, m := range msg {
		dec, err := DecryptMessage(m.SMS, store["ENCRYPTION_KEY"])
		if err != nil {
			logger.Printf("error decrypting message; %v", err)
			continue
		}
		msg[i].SMS = dec
	}

	return &msg

}
