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
      "temp": {
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
      "light": {
        "range": {
          "min": 0,
          "max": 1.0
        }
      }
    }
  }
}

fba.post("/polytunnels/create", data=data)
#  fba.post("/polytunnels/delete", data="")
#  id = "2296cf0f-d834-4d26-8d84-b0acb0d081ba"
#  fba.post(f"/polytunnels/{id}/info/devices/heater", data="")
#  fba.post("/polytunnels/intervene", data={ "uuid": "", "device": "sprinkler" })
