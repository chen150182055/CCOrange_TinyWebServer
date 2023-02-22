set(SRCS
        main.cpp
        timer/lst_timer.cpp
        http/http_conn.cpp
        log/log.cpp
        CGImysql/sql_connection_pool.cpp
        webserver.cpp
        config.cpp
        )
add_executable(webserver ${SRCS})
target_link_libraries(webserver pthread mysqlclient)