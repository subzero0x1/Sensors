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
parser.add_argument('--spreadsheetid', nargs=1, type=str, required=True, help='Google Sheet to write data to')
parser.add_argument('--googleclientsecrect', nargs=1, type=str, required=True, help='Google Sheet to write data to')
args = parser.parse_args()
mdir = args.dir[0]
spreadsheetId = args.spreadsheetid[0]
clientSecretFile = args.googleclientsecrect[0]

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

# Read sensor data in JSON format from found files
values = []
for file in files:
    logger.debug('Open file ' + file)
    f = open(normpath(join(mdir, file)), 'r')
    data = json.load(f)
    logger.debug('Loaded json ' + str(data))
    f.close()
    row = [None, None, None, None, None, None, None]
    for key in data:
        row[INDEXES[key]] = data[key]
    values.append(row)
logger.debug(values)

# Save sensor data to Google Sheet
result = service.spreadsheets().values().append(
    spreadsheetId=spreadsheetId, range='Data!A2:G',
    valueInputOption='USER_ENTERED', body={'values': values}).execute()
updates = result.get('updates')
logger.info('{0} cells in {1} rows appended to Google Sheet'
            .format(updates.get('updatedCells'), updates.get('updatedRows')))

# Delete files
for file in files:
    os.remove(normpath(join(mdir, file)))
    logger.debug('Deleted file ' + file)
logger.info('{0} files deleted'.format(len(files)))