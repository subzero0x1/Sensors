import argparse
import datetime
import json
import logging
import uuid
import serial

from os.path import normpath, join

logging.basicConfig(format='%(asctime)-15s %(message)s')
logger = logging.getLogger('serial-reader')
logger.setLevel(level=logging.INFO)
logger.info('serial-reader started')

parser = argparse.ArgumentParser(description='Device Data Serial Reader.')
parser.add_argument('--serial', nargs=1, type=str, required=True, help='serial port address')
parser.add_argument('--dir', nargs=1, type=str, required=True, help='directory to save messages')
args = parser.parse_args()
sport = args.serial[0]
mdir = args.dir[0]

logger.info('Reading from ' + sport)
ser = serial.Serial(sport, 9600)
while True:
    data = ser.readline()
    logger.debug(data)
    logger.info('{0} bytes read'.format(len(data)))
    msg = int(data)
    logger.debug(msg)
    deviceId = msg >> 28 & 0xF
    logger.debug('deviceId: ' + str(deviceId))
    curtime = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    fname = str(uuid.uuid1())
    jm = ''
    if deviceId == 0:
        jm = json.dumps({
            'deviceId': deviceId,
            'distance': (msg >> 16) & 0xFFF,
            'timestamp': curtime},
            sort_keys=True,
            indent=4)
        logger.debug(jm)
    elif deviceId == 1:
        jm = json.dumps({
            'deviceId': deviceId,
            'humidity': (msg >> 21) & 0x7F,
            'temperature': (msg >> 15) & 0x3F,
            'rain': (msg >> 13) & 0x3,
            'timestamp': curtime},
            sort_keys=True,
            indent=4)
        logger.debug(jm)
    elif deviceId == 2:
        jm = json.dumps({
            'deviceId': deviceId,
            'humidity': (msg >> 16) & 0xFFF,
            'temperature': msg & 0xFFFF,
            'timestamp': curtime},
            sort_keys=True,
            indent=4)
        logger.debug(jm)
    elif deviceId == 3:
        jm = json.dumps({
            'deviceId': deviceId,
            'pressure': (msg >> 12) & 0x3FF,
            'temperature': (msg >> 22) & 0x3F,
            'timestamp': curtime},
            sort_keys=True,
            indent=4)
        logger.debug(jm)
    else:
        logger.error('Unsupported deviceId ' + str(deviceId) + ', message: ' + str(data))
        continue
    f = open(normpath(join(mdir, fname)), "w")
    f.write(jm)
    f.close()
    logger.info('Saved sensor data to file {0}'.format(fname))