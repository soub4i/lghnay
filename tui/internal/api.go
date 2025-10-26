package internal

import (
	"context"
	"net/http"
	"os"
	"time"
)

func NewRepository() *Repository {

	store := map[string]string{
		"URL":            os.Getenv("API_URL"),
		"TOKEN":          os.Getenv("TOKEN"),
		"ENCRYPTION_KEY": os.Getenv("ENCRYPTION_KEY"),
	}
	ctx := context.WithValue(context.Background(), "store", store)
	client := createClient()
	return &Repository{
		ctx:  ctx,
		http: client,
	}
}

func createClient() *http.Client {
	transport := &http.Transport{
		MaxIdleConns:        100,
		MaxIdleConnsPerHost: 10,
		IdleConnTimeout:     90 * time.Second,
		DisableCompression:  false,
		DisableKeepAlives:   false,
	}

	return &http.Client{
		Transport: transport,
		Timeout:   30 * time.Second,
	}
}
