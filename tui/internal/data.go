package internal

import (
	"context"
	"net/http"
)

type Message struct {
	ID     string
	Sender string
	SMS    string
	TS     string
}

type Repository struct {
	ctx  context.Context
	http *http.Client
}
