#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2025 Alexander Scheglov

# Parameters
DB_NAME="testdb"  # Default database name
DB_USER="dba"     # Default database user
PGHOST="localhost" # Default PostgreSQL host
PGPORT="5432"     # Default PostgreSQL port
ENABLE_LOG=${1:-1} # 0 - enable logging, 1 - disable logging
PROJECT_DIR=$(dirname "$(readlink -f "$0")") # Project directory
LOG_FILE="$PROJECT_DIR/logs/import_data_snapshots_$(date +%Y%m%d).log"
LOCK_FILE="$PROJECT_DIR/data/import_data.cron.lock"
SQL_SCRIPT="$PROJECT_DIR/sql/import_data.sql"
SQL_SCRIPT_LA="$PROJECT_DIR/sql/import_net_and_loadavg_snapshots.sql"
DATA_FILE="$PROJECT_DIR/data/metrics.csv"
MARKER_FILE="$PROJECT_DIR/data/partitions_created_$(date +%Y%m%d).marker"

# Получаем имя текущего скрипта
SCRIPT_NAME=$(basename "$0")

# Function to log messages
log() {
    local message=$1
    local script_name=${2:-$SCRIPT_NAME} 
    if [[ "$ENABLE_LOG" == 0 ]]; then
        echo "$(date +"%Y-%m-%d %H:%M:%S") - $message [$script_name]" >> "$LOG_FILE"
    fi
}

# Function to create partitions and delete old partitions
create_partitions_if_needed() {
    if [[ ! -f "$MARKER_FILE" ]]; then
        log "Creating partitions for the day..." "partition_manager"
        # Execute the partition creation command and check its status
        if psql -h "$PGHOST" -p "$PGPORT" -U "$DB_USER" -d "$DB_NAME" -c "SELECT pgsyswatch.manage_partitions_maintenance();"; then
            # If the command is successful, create a marker file
            touch "$MARKER_FILE"
            log "Partitions created successfully." "partition_manager"
        else
            # If the command fails, log the error
            log "Error: Failed to create partitions." "partition_manager"
        fi
    fi
}

# Use flock to prevent parallel execution
(
    flock -n -E 0 200 || exit 1
    
    # Call the partition creation function
    create_partitions_if_needed
    
    # Run psql for the first SQL script and capture its output
    output=$(psql -h "$PGHOST" -p "$PGPORT" -U "$DB_USER" -d "$DB_NAME" -f "$SQL_SCRIPT" 2>&1)
    psql_exit_code=$?
    
    # Log the psql output
    if [[ "$ENABLE_LOG" == 0 ]]; then
        filtered_output=$(echo "$output" | grep "INSERT")
        log "$filtered_output" "import_data"
    fi
    
    # Run psql for the second SQL script and capture its output
    output=$(psql -h "$PGHOST" -p "$PGPORT" -U "$DB_USER" -d "$DB_NAME" -f "$SQL_SCRIPT_LA" 2>&1)
    psql_exit_code=$?
    
    # Log the psql output
    if [[ "$ENABLE_LOG" == 0 ]]; then
        filtered_output=$(echo "$output" | grep "INSERT")
        log "$filtered_output" "import_net_and_loadavg_snapshots"
    fi
    
    # Handle the result of the psql execution
    if [[ $psql_exit_code -ne 0 ]]; then
        log "Error: Database is unavailable or the script failed." "main_script"
        exit 1
    fi
) 200>"$LOCK_FILE"
