#!/bin/sh
gcc main.c -o snakeforge -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 && echo "Built ./snakeforge"
