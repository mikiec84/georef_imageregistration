


import os, sys
import subprocess
import sqlite3
#from pysqlite2 import dbapi2 as sqlite3

import registration_common
import register_image

# Hard coded paths!
DB_PATH          = '/home/smcmich1/db.sqlt'
DCRAW_PATH       = '/home/smcmich1/repo/dcraw/dcraw'
RAW_IMAGE_FOLDER = '/media/network/ImagesDrop/RawESC'
TEMP_FOLDER      = '/home/smcmich1/temp_images'


# TODO: Move some of these functions up

def getFrameString(mission, roll, frame):
    return (mission +', '+ roll +', '+ frame)


# TODO: Play around with this to get the best possible output images
def convertRawFileToTiff(rawPath, outputPath):
    '''Convert one of the .raw image formats to a .tif format using
       the open-source dcraw software.'''
    if not os.path.exists(rawPath):
        raise Exception('Raw image file does not exist: ' + rawPath)
    cmd = DCRAW_PATH + ' +M -W -c -o 1 -T ' + rawPath + ' > ' + outputPath
    print cmd
    os.system(cmd)
    if not os.path.exists(outputPath):
        raise Exception('Failed to convert input file ' + rawPath)

def chooseSubFolder(frame, cutoffList, pathList):
    '''Helper function to reduce boilerplate in getRawPath.
       Choose which subfolder a file is in based on the frame number.'''
    numVals = len(cutoffList)
    if not (len(pathList)-1) == numVals:
        raise Exception('Bad cutoff list!')
    for i in range(0,numVals):
        if frame < cutoffList[i]:
            return pathList[i]
    return pathList[-1]

def getRawPath(mission, roll, frame):
    '''Generate the full path to a specified RAW file.'''
    
    # The file storage system is not consistent, so
    #  many missions need some special handling.
    
    # Zero pad values as needed
    FRAME_DIGITS = 6
    zFrame = frame.rjust(FRAME_DIGITS, '0')
    
    # The default conventions
    name      = mission.lower() + roll.lower() + zFrame
    ext       = '.nef'
    subFolder = mission # The mission name in caps
    iFrame = int(frame)
    
    if ( (mission == 'ISS006') or
         (mission == 'ISS011') or
         (mission == 'ISS012') or
         (mission == 'ISS013') or
         (mission == 'ISS014') or
         (mission == 'ISS016') or
         (mission == 'ISS017')):
        name = mission + roll + zFrame
        ext  = '.dcr'
    
    if ( (mission == 'ISS015') or
         (mission == 'ISS014')):
        name = mission + roll + zFrame
        ext  = '.DCR'

    if ( (mission == 'ISS018') or
         (mission == 'ISS019') or
         (mission == 'ISS019')):
        name = mission + roll + zFrame

    if ( (mission == 'ISS032') or
         (mission == 'ISS033') or
         (mission == 'ISS034') or
         (mission == 'ISS035') or
         (mission == 'ISS036') or
         (mission == 'ISS037') or
         (mission == 'ISS038') or
         (mission == 'ISS039') or
         (mission == 'ISS040') or
         (mission == 'ISS041') or
         (mission == 'ISS042') or
         (mission == 'ISS046')):
        ext  = '.NEF'

    if mission == 'ISS022':
        subFolder = 'ISS022/' + chooseSubFolder(iFrame,
            [46575, 60274, 66153, 72633, 78049, 81235, 87189, 99095, 102079, 102303],
            ['e031971-e046574', 'e046575-e060273', 'e060274-e066152',
             'e066153-e072632', 'e072633-e078048', 'e078049-e081234',
             'e081235-e087188', 'e087189-e099094', 'e099095-e102078',
             'e102079-e102302', 'e102303-e102923'])

    if mission == 'ISS023':
        subFolder = 'ISS023/' + chooseSubFolder(iFrame,
            [8826, 14780, 19842, 24906, 29099, 34136,
             40035, 45624, 51324, 58386, 58480],
            ['e005000-e008825', 'e008826-e014779', 'e014780-e019841',
             'e019842-e024905', 'e024906-e029098', 'e029099-e034135',
             'e034136-e040034', 'e040035-e045623', 'e045624-e051323',
             'e051324-e058385', 'e058386-e058479', 'e058480-e060753'])
    
    if mission == 'ISS030':
        ext  = '.NEF'
        subFolder = chooseSubFolder(iFrame,
            [107724, 209872, 228051],
            ['ISS030', 'ISS030_Batch02', 'ISS030_Batch03', 'ISS030_Batch04'])

    if mission == 'ISS031':
        ext  = '.nef'
        subFolder = chooseSubFolder(iFrame, [95961], ['ISS031', 'ISS031_Batch02'])

    if mission == 'ISS042':
        ext  = '.NEF'
        subFolder = chooseSubFolder(iFrame,
            [170284, 280908], ['ISS042', 'ISS042_Batch02', 'ISS030_Batch03'])

    if mission == 'ISS043':
        ext  = '.NEF'
        subFolder = chooseSubFolder(iFrame, [159293], ['ISS043', 'ISS043_Batch02'])
        
    if mission == 'ISS045':
        ext  = '.NEF'
        subFolder = chooseSubFolder(iFrame, [152311], ['ISS045', 'ISS045_Batch02'])
    
    subPath  = os.path.join(subFolder, name+ext)
    fullPath = os.path.join(RAW_IMAGE_FOLDER, subPath)
    return fullPath

def getSensorSize(cameraCode):
    '''Returns sensor (width, height) in mm given the two letter
       camera code from the database.'''
    if cameraCode == 'E2':
        return (27.6, 18.5)
    if cameraCode == 'E3':
        return (27.6, 18.5)
    if cameraCode == 'E4':
        return (27.6, 18.5)
    if cameraCode == 'N1':
        return (23.6, 15.5)
    if cameraCode == 'N2':
        return (23.7, 15.7)
    if cameraCode == 'N3':
        return (36.0, 23.9)
    if cameraCode == 'N4':
        return (35.9, 24.0)
    if cameraCode == 'N5':
        return (36.0, 23.9)
    if cameraCode == 'N6':
        return (36.0, 23.9)
    if cameraCode == 'N7':
        return (35.9, 24.0)
    print 'Warning: Missing sensor code for camera ' + cameraCode
    return (0,0)


def getRawImageSize(rawPath):
    '''Returns the size in pixels of the raw camera file'''

    cmd = [DCRAW_PATH, '-i',  '-v', rawPath]
    print cmd
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    textOutput, err = p.communicate()
    lines = textOutput.split('\n')
    for line in lines:
        if 'Full size' not in line:
            continue
        parts  = line.split()
        width  = int(parts[2])
        height = int(parts[4])
        return (width, height)
    raise Exception('Unable to determine size of image: ' + rawPath)
    

class FrameInfo(object):
    '''Class that contains the metadata for one frame.'''
    
    def __init__(self):
        '''Init to default values'''
        self.mission         = None
        self.roll            = None
        self.frame           = None
        self.centerLon       = None
        self.centerLat       = None
        self.nadirLon        = None
        self.nadirLat        = None
        self.altitude        = None
        self.date            = None
        self.time            = None
        self.focalLength     = None
        self.exposure        = 'N'
        self.tilt            = 'NV'
        self.cloudPercentage = 0.0
        self.camera          = None
        self.imageList       = []
        self.rawPath         = None
        self.width           = 0
        self.height          = 0
        self.sensorHeight    = 0
        self.sensorWidth     = 0
        
    def __str__(self):
        '''Print out the values'''
        return str(self.__dict__)
        
    def loadFromDb(self, dbCursor, mission, roll, frame):
        '''Populate from an entry in the database'''
        
        dbCursor.execute('select * from Frames where trim(MISSION)=? and trim(ROLL)=? and trim(FRAME)=?',
                       (mission, roll, frame))
        rows = dbCursor.fetchall()
        if len(rows) != 1: # Make sure we found the next lines
            raise Exception('Could not find any data for frame: ' +
                            getFrameString(mission, roll, frame))

        rows = rows[0]
        print rows
        self.mission         = mission
        self.roll            = roll
        self.frame           = frame
        self.exposure        = str(rows[0]).strip()
        self.tilt            = str(rows[3]).strip()
        self.time            = str(rows[8]).strip()
        self.date            = str(rows[9]).strip()
        self.cloudPercentage = float(rows[13]) / 100
        self.altitude        = float(rows[15])
        self.focalLength     = float(rows[18])
        self.centerLat       = float(rows[19])
        self.centerLon       = float(rows[20])
        self.nadirLat        = float(rows[21])
        self.nadirLon        = float(rows[22])
        self.camera          = str(rows[23]).strip()
        self.film            = str(rows[24]).strip()
        
        # The input tilt can be in letters or degrees so convert
        #  it so that it is always in degrees.
        if (self.tilt == 'NV') or not self.tilt:
            self.tilt = '0'
        if self.tilt == 'LO':
            self.tilt = '20'
        if self.tilt == 'HO':
            self.tilt = '70'
        self.tilt = float(self.tilt)
       
        # Convert the date to 'YYYY.MM.DD' format that the image fetcher wants
        # - TODO: Use a standardized format!
        self.date = self.date[0:4] + '.' + self.date[4:6] + '.' +  self.date[6:8]
       
        # Get the sensor size
        (self.sensorWidth, self.sensorHeight) = getSensorSize(self.camera)
       
        #if not self.isGoodAlignmentCandidate():
        #    return # In this case don't bother finding the images
    
        # Fetch the associated non-raw image files
        dbCursor.execute('select * from Images where trim(MISSION)=? and trim(ROLL)=? and trim(FRAME)=?',
                           (mission, roll, frame))
        rows = dbCursor.fetchall()
        if len(rows) < 1: # No images provided
            return
        # Record the image paths
        bestNumPixels = 0
        for row in rows:
            # Get the file path and verify it exists
            folder = str(row[4]).strip()
            name   = str(row[5]).strip()
            path   = os.path.join(folder, name)
            if not os.path.exists(path):
                continue           
            self.imageList.append(path)
            
            # Record if this is the highest resolution image
            width     = int(row[6])
            height    = int(row[7])
            numPixels = width*height
            if numPixels > bestNumPixels:
                self.width     = width
                self.height    = height
        
        # Try to find an associated RAW file
        thisRaw = getRawPath(self.mission, self.roll, self.frame)
        if os.path.exists(thisRaw):
            self.rawPath = thisRaw
        else:
            print 'Did not find: ' + thisRaw
        
        # TODO: Handle images with no RAW data
        # Get the image size
        if self.rawPath:
            (self.width, self.height) = getRawImageSize(self.rawPath)
        
    # These functions check individual metadata parameters to
    #  see if the image is worth trying to process.
    def isExposureGood(self):
        return (self.exposure == 'N') or (self.exposure == '')
    def isTiltGood(self):
        MAX_TILT_ANGLE = 50
        return (self.tilt < MAX_TILT_ANGLE)
    def isCloudPercentageGood(self):
        MAX_CLOUD_PERCENTAGE = 0.20
        return (self.cloudPercentage < MAX_CLOUD_PERCENTAGE)
    
    def isGoodAlignmentCandidate(self):
        '''Return True if the image is a good candidate for automatic alignment'''
        return (self.isExposureGood() and
                self.isTiltGood() and
                self.isCloudPercentageGood() and
                self.rawPath) # Later, work with non-raw images.

    


def main():
    pass

def test():

    # Open the database
    print 'Initializing database connection...'
    db     = sqlite3.connect(DB_PATH)
    cursor = db.cursor()
    
    print 'Fetching frame information...'
    frame = FrameInfo()
    #frame.loadFromDb(cursor, 'ISS001', '347', '24')
    #frame.loadFromDb(cursor, 'ISS026', 'E', '29592')
    
    frame.loadFromDb(cursor, 'ISS043', 'E', '91884') # Warp trouble?
    #frame.loadFromDb(cursor, 'ISS043', 'E', '93251') # Works poorly on snow!
    #frame.loadFromDb(cursor, 'ISS043', 'E', '122588') # Good only on lake
    #frame.loadFromDb(cursor, 'ISS044', 'E', '868')   # Should be better
    #frame.loadFromDb(cursor, 'ISS044', 'E', '1998') # Tough image
    
    if not frame.isGoodAlignmentCandidate():
        print 'This image is not a valid alignment candidate:'
        print frame
        return -1
    
    print frame
    
    # TODO: Make sure everything properly handles existing data!
    
    # We can't operate on the RAW file, so convert it to TIF.
    print 'Converting RAW to TIF...'
    tempName = 'raw_converted.tif'
    tempPath = os.path.join(TEMP_FOLDER, tempName)
    convertRawFileToTiff(frame.rawPath, tempPath)
    
    # Estimate the pixel resolution on the ground
    metersPerPixel = registration_common.estimateGroundResolution(frame.focalLength,
                            frame.width, frame.height, frame.sensorWidth, frame.sensorHeight,
                            frame.nadirLon, frame.nadirLat, frame.altitude,
                            frame.centerLon, frame.centerLat)
    print 'Meters per pixel = ' + str(metersPerPixel)
    
    # Try to register the image
    print 'Attempting to register image...'
    register_image.register_image(tempPath,
                                  frame.centerLon, frame.centerLat,
                                  metersPerPixel, frame.date,
                                  refImagePath=None, debug=True, force=False)

    # Clean up
    
    print 'Closing database connection...'
    db.close()
    
    print 'Finished running backlog test'


# Simple test script
if __name__ == "__main__":
    sys.exit(test())