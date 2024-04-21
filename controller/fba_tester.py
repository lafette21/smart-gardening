#!/usr/bin/env python3

from firebase import firebase


fba = firebase.FirebaseApplication("https://ertos-2024-default-rtdb.europe-west1.firebasedatabase.app/", None)

data = {
  "crop": {
    "name": "weed",
    "attributes": {
      "soilMoisture": {
        "range": {
          "min": 0,
          "max": 1.0
        }
      },
      "temperature": {
        "range": {
          "min": 23.4,
          "max": 34.4
        }
      },
      "humidity": {
        "range": {
          "min": 0,
          "max": 1.0
        }
      },
      "brightness": {
        "range": {
          "min": 0,
          "max": 1.0
        }
      }
    }
  }
}

fba.post("/polytunnels/create", data=data)
