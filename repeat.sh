#!/bin/bash

while true; do
    bash scripts/gen_testing.sh

    if [ $? -eq 0 ]; then
        break
    fi
done