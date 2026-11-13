#!/bin/bash

valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --num-callers=50 \
    --error-exitcode=99 \
    --log-file=./read_stat-valgrind.log \
    duckdb -unsigned \
    -c "LOAD 'build/debug/read_stat.duckdb_extension'; SELECT * FROM read_stat('test/sql/sample.sav');"


