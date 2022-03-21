package main

import (
	"log"

	"pogreb"
)

func main() {
    db, err := pogreb.Open("fighters.z", nil)

    db.Put([]byte("vegeta"), []byte("final_flash"))
    db.Put([]byte("hit"), []byte("time_skip"))
    db.Put([]byte("goku"), []byte("kamehameha"))

    if err != nil {
        log.Fatal(err)
        return
    }	

    defer db.Close()
}
