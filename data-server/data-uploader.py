from __future__ import print_function

import argparse
import json
import logging
import os
import sys
from os import walk
from os.path import normpath, join

import httplib2
from apiclient import discovery
from oauth2client import client
from oauth2client import tools
from oauth2client.file import Storage


# Constants
INDEXES = {
    'timestamp': 0,
    'deviceId': 1,
    'temperature': 2,
    'humidity': 3,
    'pressure': 4,
    'rain': 5,
    'distance': 6
}

# Logger
logging.basicConfig(format='%(asctime)-15s %(message)s')
logger = logging.getLogger('data-uploader')
logger.setLevel(level=logging.INFO)
logging.getLogger('googleapiclient.discovery_cache').setLevel(logging.ERROR)
logger.info('data-uploader started')

# Argument Parser
parser = argparse.ArgumentParser(description='Device Data Uploader.', parents=[tools.argparser])
parser.add_argument('--dir', nargs=1, type=str, required=True, help='directory to read messages from')
parser.add_argument('--cachedir', nargs=1, type=str, required=True, help='directory to cache messages')
parser.add_argument('--spreadsheetid', nargs=1, type=str, required=True, help='Google Sheet to write data to')
parser.add_argument('--googleclientsecret', nargs=1, type=str, required=True, help='Google Sheet to write data to')
args = parser.parse_args()
mdir = args.dir[0]
cdir = args.cachedir[0]
spreadsheetId = args.spreadsheetid[0]
clientSecretFile = args.googleclientsecret[0]

# Google Client
home_dir = os.path.expanduser('~')
credential_dir = os.path.join(home_dir, '.credentials')
if not os.path.exists(credential_dir):
    os.makedirs(credential_dir)
credential_path = os.path.join(credential_dir,
                               'sheets.googleapis.com-python-sensor-uploader.json')
store = Storage(credential_path)
credentials = store.get()
if not credentials or credentials.invalid:
    flow = client.flow_from_clientsecrets(clientSecretFile,
                                          'https://www.googleapis.com/auth/spreadsheets')
    flow.user_agent = 'Sensor Data Uploader'
    if args:
        credentials = tools.run_flow(flow, store, args)
    else:  # Needed only for compatibility with Python 2.6
        credentials = tools.run(flow, store)
    logger.debug('Storing credentials to ' + credential_path)

http = credentials.authorize(httplib2.Http())
discoveryUrl = ('https://sheets.googleapis.com/$discovery/rest?'
                'version=v4')
service = discovery.build('sheets', 'v4', http=http, discoveryServiceUrl=discoveryUrl)

# Search for files
logger.debug('Walking to dir ' + mdir)
files = []
for (dirpath, dirnames, filenames) in walk(mdir):
    files.extend(filenames)
    break
logger.debug('Found files: ' + str(files))
if len(files) == 0:
    logger.info('No files found')
    sys.exit()
else:
    logger.info(str(len(files)) + ' files found')

# Search for cached files
logger.debug('Walking to cache dir ' + cdir)
cfiles = []
for (cdirpath, cdirnames, cfilenames) in walk(cdir):
    cfiles.extend(cfilenames)
    break
logger.debug('Found files: ' + str(cfiles))
if len(cfiles) == 0:
    logger.info('No cached files found')
else:
    logger.info(str(len(cfiles)) + ' cached files found')

# read cached data from cached data dir
cvalues = {}
for cfile in cfiles:
    logger.debug('Open file ' + cfile)
    f = open(normpath(join(cdir, cfile)), 'r')
    try:
        data = json.load(f)
        logger.debug('Loaded cached json ' + str(data))
    except ValueError as e:
        logger.error(str(e) + ' ' + cfile)
    else:
        cvalues[data['deviceId']] = data
    f.close()
logger.debug(cvalues)

# Read sensor data in JSON format from found files
from shutil import copyfile
values = []
for file in files:
    logger.debug('Open file ' + file)
    f = open(normpath(join(mdir, file)), 'r')
    try:
        data = json.load(f)
        logger.debug('Loaded json ' + str(data))
        deviceId = data['deviceId']
        cdata = cvalues.get(deviceId, None)
        if cdata is not None:
            if deviceId == 0:
                if int(int(data['distance'])/10) - int(int(cdata['distance'])/10) == 0:
                    logger.debug('Ignore sensor data. New data: ' + str(data) + ', cached data: ' + str(cdata))
                    continue
            elif deviceId == 1:
                #deltaTemp = int(data['temperature']) - int(cdata['temperature'])
                #deltaHum = int(data['humidity']) - int(cdata['humidity'])
                deltaRain = int(data['rain']) - int(cdata['rain'])
                if deltaRain == 0: # 2 > deltaTemp > -2 and 10 > deltaHum > -10 and
                    logger.debug('Ignore sensor data. New data: ' + str(data) + ', cached data: ' + str(cdata))
                    continue
            elif deviceId == 2:
                deltaTemp = int(data['temperature']) - int(cdata['temperature'])
                deltaHum = int(data['humidity']) - int(cdata['humidity'])
                if 5 > deltaTemp > -5 and 10 > deltaHum > -10:
                    logger.debug('Ignore sensor data. New data: ' + str(data) + ', cached data: ' + str(cdata))
                    continue
            elif deviceId == 3:
                deltaTemp = int(data['temperature']) - int(cdata['temperature'])
                deltaPress = int(data['pressure']) - int(cdata['pressure'])
                if 2 > deltaTemp > -2 and 2 > deltaPress > -2:
                    logger.debug('Ignore sensor data. New data: ' + str(data) + ', cached data: ' + str(cdata))
                    continue
            else:
                logger.error('Unsupported deviceId in cached data: ' + str(deviceId))
        cvalues[data['deviceId']] = data
        copyfile(normpath(join(mdir, file)), normpath(join(cdir, str(deviceId) + '.json')))
    except ValueError as e:
        logger.error(str(e) + ' ' + file)
    else:    
        row = [None, None, None, None, None, None, None]
        for key in data:
            row[INDEXES[key]] = data[key]
        values.append(row)
    f.close()
logger.debug(values)

# Save sensor data to Google Sheet
if len(values) > 0:
    result = service.spreadsheets().values().append(
        spreadsheetId=spreadsheetId, range='Data!A2:G',
        valueInputOption='USER_ENTERED', body={'values': values}).execute()
    updates = result.get('updates')
    logger.info('{0} cells in {1} rows appended to Google Sheet'
                .format(updates.get('updatedCells'), updates.get('updatedRows')))
else:
    logger.info('Nothing new to submit to Google Sheet')

# Delete files
for file in files:
    os.remove(normpath(join(mdir, file)))
    logger.debug('Deleted file ' + file)
logger.info('{0} files deleted'.format(len(files)))
