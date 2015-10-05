#!/bin/bash


mysql -u root -p < ./poesis.sql
mysql -u root -p poesis < ./TableDef_pagini-html.sql
mysql -u root -p poesis < ./TableDef_referinte-html.sql
mysql -u root -p poesis < ./TableDef_referinte-globale.sql

