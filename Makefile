# SPDX-License-Identifier: Apache-2.0
# Copyright 2025 Alexander Scheglov
# Variables
EXTENSION = pgsyswatch
DATA = sql/pgsyswatch--1.0.sql
DB_NAME = testdb  # Default database name
DB_USER = dba     # Default database user
# Source files (Search for all .c files)
SRCS = $(wildcard src/*.c)
# Object files (Replace .c with .o)
OBJS = $(SRCS:.c=.o)
LIBS = pgsyswatch.so
# Compiler settings
PG_CONFIG = pg_config
PG_CPPFLAGS = -I/usr/include/postgresql/15/server
CFLAGS = -g -fPIC -Wall -Werror
# Build rules
all: $(LIBS)
pgsyswatch.so: $(OBJS)
	$(CC) -shared -o $@ $^
%.o: %.c
	$(CC) $(CFLAGS) $(PG_CPPFLAGS) -c $< -o $@
# Installation
install: all
	sudo systemctl stop postgresql-custom
	sudo cp $(LIBS) /usr/local/pgsql/lib/
	sudo cp $(DATA) /usr/local/pgsql/share/extension/
	sudo cp pgsyswatch.control /usr/local/pgsql/share/extension/
	sudo chown postgres:postgres /usr/local/pgsql/lib/*.so
	sudo chown postgres:postgres /usr/local/pgsql/share/extension/*
	sudo systemctl start postgresql-custom
# Clean
clean:
	rm -f src/*.o $(LIBS)
# Reload PostgreSQL (optional)
reload:
	sudo systemctl restart postgresql-custom
# Stop PostgreSQL (optional)
pgstop:
	sudo systemctl stop postgresql-custom
# Start PostgreSQL (optional)
pgstart:
	sudo systemctl start postgresql-custom
# Testing (optional)
.PHONY: test
test:
	@echo "Please wait 2 seconds while start postgresql &  the extension is being installed..."
	sleep 3
	psql -U $(DB_USER) -h $(PGHOST) -p $(PGPORT) -d $(DB_NAME) -c "drop extension if exists pgsyswatch cascade;"
	sleep 3
	psql -U $(DB_USER) -h $(PGHOST) -p $(PGPORT) -d $(DB_NAME) -c "create extension pgsyswatch;"
	psql -U $(DB_USER) -h $(PGHOST) -p $(PGPORT) -d $(DB_NAME) -c "SELECT * FROM pgsyswatch.proc_activity_snapshots;"
	psql -U $(DB_USER) -h $(PGHOST) -p $(PGPORT) -d $(DB_NAME) -c "SELECT * FROM pgsyswatch.pg_proc_activity;"
