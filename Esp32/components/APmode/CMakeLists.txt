idf_component_register(SRCS "APmode.c"
                       PRIV_REQUIRES esp_wifi nvs_flash wifi_provisioning
                       INCLUDE_DIRS "include"
                       EMBED_TXTFILES "ap_index.html"
                       EMBED_FILES "images/ap_favicon_32x32.png" "images/ap_logoBK.png"
                       REQUIRES esp_http_server)