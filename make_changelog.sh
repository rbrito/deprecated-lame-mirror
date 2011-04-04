#!/bin/sh

cvs2cl --stdin --gmt -S --branches --no-wrap --header ChangeLog.header <../log_output
