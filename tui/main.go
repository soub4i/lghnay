package main

import (
	"github.ibm.com/soub4i/lghnay-tui/internal"
)

func main() {
	logger := internal.GetLogger()
	logger.Println("Welcome to lghnay TUI")

	app := internal.NewApp()

	logger.Println("Fetching...")
	r := internal.NewRepository()
	el := r.GetMessages()
	app.SetElements(el)

	// Start the TUI application
	app.Render()

}
