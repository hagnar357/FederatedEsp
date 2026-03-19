cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(meu_lora)

# Inclui o diretório de cabeçalhos
target_include_directories(app PRIVATE
    include
)

# # Subdiretórios
# add_subdirectory(src/federatedfearning)
# add_subdirectory(src/http)
# add_subdirectory(src/CJSON)
# add_subdirectory(src/websocket)

# add_subdirectory(src/espconfiguration)

# Adiciona os arquivos fonte do projeto
target_sources(app PRIVATE 
    src/main.c #problema com timer
    src/cJSON.c
    src/espconfiguration.c
    src/federatedlearning.c # log talvez e, float e double
    src/httpclient.c
    src/JSONConverter.c
    src/websocketclient.c
)
