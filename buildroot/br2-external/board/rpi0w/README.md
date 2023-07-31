### Setup WIFI password

Replace `*******` inside the `wpa_supplicant.conf` with correct credentials:

```
network={
    ssid="*******"
    psk="*******"
}
```

Execute the command below to stop tracking the `wpa_supplicant.conf`

`git update-index --assume-unchanged wpa_supplicant.conf`
