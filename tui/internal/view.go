package internal

import (
	"fmt"

	"github.com/gdamore/tcell/v2"
	"github.com/rivo/tview"
)

type App struct {
	app               tview.Application
	elemets           *[]Message
	elemetsView       *tview.List
	currentElemetView *tview.TextView
}

var logger = GetLogger()

func NewApp() *App {
	msgList := tview.NewList().ShowSecondaryText(false)
	msgList.SetBorder(true).SetTitle("Inbox")

	msg := tview.NewTextView().
		SetDynamicColors(true).
		SetRegions(true).
		SetWordWrap(true)

	msg.SetBorder(true).SetTitle("View")

	return &App{
		app:               *tview.NewApplication(),
		elemets:           nil,
		elemetsView:       msgList,
		currentElemetView: msg,
	}
}

func (a *App) Render() error {
	logger.Println("Rendering UI")

	flex := tview.NewFlex().
		AddItem(a.elemetsView, 0, 1, true).
		AddItem(a.currentElemetView, 0, 1, false)

	flex.SetInputCapture(func(event *tcell.EventKey) *tcell.EventKey {
		if event.Rune() == rune('r') {

			a.Refresh()
		}
		if event.Rune() == rune('q') || event.Rune() == rune(tcell.KeyEscape) {
			a.app.Stop()
		}
		return event
	})

	footer := tview.NewForm().AddButton("(R)efresh", nil).AddButton("(Q)uit", func() {
	})

	grid := tview.NewGrid().
		SetRows(15, 3).
		SetBorders(true).
		AddItem(flex, 0, 0, 1, 3, 0, 0, true).
		AddItem(footer, 1, 0, 1, 3, 0, 0, false)

	// Layout for screens narrower than 100 cells (menu and side bar are hidden).

	// Layout for screens wider than 100 cells.
	// grid.AddItem(menu, 1, 0, 1, 1, 0, 100, false).
	// AddItem(main, 1, 1, 1, 1, 0, 100, false).
	// AddItem(sideBar, 1, 2, 1, 1, 0, 100, false)

	a.Refresh()
	return a.app.SetRoot(grid, true).Run()
}

func (a *App) SetElements(e *[]Message) {
	a.elemets = e
}

func (a *App) Refresh() {
	a.elemetsView.Clear()
	a.currentElemetView.Clear()
	if a.elemets == nil || len(*a.elemets) == 0 {
		a.elemetsView.AddItem("No SMS.", "", 0, nil)
	} else {
		for i, item := range *a.elemets {
			a.elemetsView.AddItem(fmt.Sprintf("[%d] %s - %s (SMS: %s)\n", i+1, item.Sender, item.TS, truncateText(item.SMS, 10)), "", rune(i+1), func() {
				a.currentElemetView.Clear()
				fmt.Fprintf(a.currentElemetView, "[%s] %s - %s\n", item.TS, item.Sender, item.SMS)
			})
		}
	}
}

func truncateText(s string, max int) string {
	return fmt.Sprintf("%s...", s[:max])
}
