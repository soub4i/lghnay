package internal

import (
	"log"
	"os"
)

func GetLogger() *log.Logger {
	return log.New(os.Stdout, "[Lghnay]: ", log.LstdFlags)
}
