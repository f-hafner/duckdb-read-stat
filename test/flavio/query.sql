LOAD 'build/debug/read_stat.duckdb_extension';
.timer on
SELECT wage, date FROM read_stat("test/flavio/test_data.sav"); 
.quit
