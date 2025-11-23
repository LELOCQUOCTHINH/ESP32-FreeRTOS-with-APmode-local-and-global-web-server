idf_component_register(SRCS "APmode.c"
                       PRIV_REQUIRES esp_wifi nvs_flash wifi_provisioning
                       INCLUDE_DIRS "include"
                       EMBED_TXTFILES "index.html"
                       EMBED_FILES "images/favicon-32x32.png" "images/logoBK.png"
                       REQUIRES esp_http_server)