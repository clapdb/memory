#! /usr/bin/env bash

#! run memory_test for 1000 times

for i in {1..1000}; do
    ./memory_test || exit 1
done
