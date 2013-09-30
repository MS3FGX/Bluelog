#!/usr/bin/python
# -*- coding: utf-8 -*-
# deliver remotely the data gathered by the probe

import sqlite3 as lite
import sys
import requests
from datetime import datetime, timedelta
import json

url_get  = "http://ds.integreen-life.bz.it/odds/json"
url_post = "http://ds.integreen-life.bz.it/odds/json"
station_id = 13

if len(sys.argv) != 2: 
	print 'Sqlite db name required'
	exit(1)

db_name = sys.argv[1]
con = lite.connect(db_name)

with con:
	cur = con.cursor()    
	cur.execute('SELECT SQLITE_VERSION()')
	data = cur.fetchone()
	#print "SQLite version: %s" % data
	con.row_factory = lite.Row
	cur = con.cursor()    
	cur.execute("PRAGMA synchronous = OFF")

	# Make a GET to request the last record stored so far
	url_vars = {'station-id':station_id}
	r = requests.get(url_get, params=url_vars)
	if r.status_code != 200:
		exit(1)
	left_date = datetime.fromtimestamp( int(r.text)/1000 )
	#print 'date', left_date

	# Query the local db to retrive the last records
	#print 'Date ', left_date
	cur.execute("SELECT * FROM record WHERE gathered_on > :left_date", {'left_date': left_date.strftime('%m/%d/%y %H:%M:%S')})
	rows = cur.fetchall()

	values = [ {'station_id': station_id, "local_id":row['id'], 'mac':row['mac'], 'gathered_on': datetime.strptime(row['gathered_on'],'%m/%d/%y %H:%M:%S').strftime('%Y-%m-%dT%H:%M:%S.000+02:00') } for row in rows]

	# Make a POST to send the last record
	headers = {'Content-type': 'application/json', 'Accept': 'text/plain'}
	r = requests.post( url_post, data=json.dumps(values), headers=headers )
	if r.status_code == 200:
		cur.execute("DELET FROM record WHERE gathered_on < :left_date", {'left_date': left_date.strftime('%m/%d/%y %H:%M:%S')})

