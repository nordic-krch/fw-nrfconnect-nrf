
zephyr_include_directories_ifdef(CONFIG_UNITY
        ${ZEPHYR_BASE}/../cmock/vendor/unity/src 
        ${ZEPHYR_BASE}/../cmock/src)
zephyr_sources_ifdef(CONFIG_UNITY
	${ZEPHYR_BASE}/../cmock/vendor/unity/src/unity.c
	${ZEPHYR_BASE}/../cmock/src/cmock.c
	)
