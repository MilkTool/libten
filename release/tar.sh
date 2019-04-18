#!/bin/env bash
tar -czvf $1.tgz --exclude=.[^.]* --exclude=release ..
