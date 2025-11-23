idf_component_register(SRCS "STAmode.c"
                    PRIV_REQUIRES esp_wifi nvs_flash wifi_provisioning
                    INCLUDE_DIRS "include"
                    EMBED_TXTFILES "sta_index.html"
                    EMBED_FILES "images/sta_favicon_32x32.png" "images/sta_logoBK.png"
                    REQUIRES mqtt esp_http_server)