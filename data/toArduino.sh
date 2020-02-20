#!/bin/sh

minify index.html | sed 's/"/\\"/g' - | tr -d '\n' > index2.html
