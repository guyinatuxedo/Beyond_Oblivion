package main

import (
	"log"

	"pogreb"
)

func main() {
    db, err := pogreb.Open("fighters.z", nil)

    val, er1 := db.Get([]byte("vegeta"))

    if err != nil {
        log.Fatal(err)
        return
    }	

    if er1 != nil {
        log.Fatal(err)
        return
    }

    log.Printf("%s", val)

    defer db.Close()
}
