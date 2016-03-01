import numpy
import sys
import threading
import time
import json
import requests
import SimpleHTTPServer
import SocketServer
import cgi
import psutil
import zlib
from datetime import datetime

ping = __import__('ping')

BOOTSTRAP   = 0
EXECUTION   = 1
AGGREGATION = 2
TERMINATION = 3

STATETRANSFERPERIOD = 0.5
CPUUPDATEPERIOD = 2
JOBTRANSFERCHECKPERIOD = 0.2
JOBITERATION = 80
#JOBITERATION = 10
DELAYTHRESHOLD = 0.19

port = 8080
remote_addr = '172.22.156.79'
local_addr = '172.22.156.19'
#remote = 'http://10.0.0.11:' + `port`
#local = 'http://10.0.0.10:' + `port`
remote = 'http://' + remote_addr + ':' + `port`
local = 'http://' + local_addr + ':' + `port`
phase = BOOTSTRAP

minJobNum    = 512
allocateSize = 1024 * 1024 * 32
jobList      = []
resultList   = []
throttling = float(sys.argv[2])
isLocal = sys.argv[1] == 'local'
isRemoteTerminate = False
jobListLock = threading.Lock()
jobTransfered = 0
additionWorkerThread = 0
resultA = []
if isLocal:
    resultA = [0] * allocateSize

class MP4Job:
    ID = 0
    startPoint = 0
    A = []
    def __init__(self, ID, start, A):
        self.ID= ID
        self.startPoint = start
        self.A = A
    def toObj(self):
        return {'__type__': 'MP4Job', 'ID': self.ID, 'startPoint': self.startPoint, 'A': self.A}

# support for the json decoder to decode object MP4Job
def job_decoder(obj):
    if '__type__' in obj and obj['__type__'] == 'MP4Job':
        return MP4Job(obj['ID'], obj['startPoint'], obj['A'])
    return obj

def getStateObjToBrowser(stateObj):
    stateObj['resultNum'] = len(resultList)
    return stateObj

class ServerHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def sendHeader(self):
        self.protocol_version= 'HTTP/1.1'
        self.send_response(200, 'OK')
        self.send_header('Content-type', 'application/json')
        self.end_headers()
    def do_GET(self):
        key = self.path[1:].split('/')
        if key[0] == 'state':
            self.sendHeader()
            self.wfile.write(json.dumps(getStateObjToBrowser(stateManager.stateToObj())))
        elif key[0] == 'jobTransfered':
            self.sendHeader()
            global jobTransfered
            self.wfile.write(json.dumps({'jobTransfered': jobTransfered}))
            jobTransfered = 0
        elif key[0] == 'throttling':
            #print 'throttling'
            #print key[1]
            global throttling
            throttling = float(key[1])
    def do_POST(self):
        # parse request into form
        form = cgi.FieldStorage(
            fp=self.rfile,
            headers=self.headers,
            environ={'REQUEST_METHOD':'POST',
                     'CONTENT_TYPE':self.headers['Content-Type'],
                     })
        for key in form.keys():
            val = form.getvalue(key)
            # receive a MP4Job
            if key == 'job':
                jobListLock.acquire()
                jobList.append(json.loads(zlib.decompress(val), object_hook=job_decoder))
                jobListLock.release()
            # receive a signal
            elif key == 'signal':
                # remote is terminated (only local will receive the signal of TERMINATION)
                if int(val) == TERMINATION:
                    global isRemoteTerminate
                    isRemoteTerminate = True
                else:
                    global phase
                    phase = int(val)
            # receive a result which is an object of type MP4Job (only local will receive result)
            elif key == 'result':
                resultList.append(json.loads(zlib.decompress(val), object_hook=job_decoder))
            # receive a state from the partner's StateManager
            elif key == 'state':
                stateManager.setPartnerVariables(json.loads(val))
        SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

class TransferManager():
    # transfer a job to the partner
    def transferJobs(self, job):
        compress = zlib.compress(json.dumps(job.toObj()))
        if isLocal:
            requests.post(remote, data={'job': compress})
        else:
            requests.post(local, data={'job': compress})
        if phase is EXECUTION:
            global jobTransfered
            jobTransfered += 1

# open a SimpleHTTPServer to listen to any request
def startListenThread():
    httpd = SocketServer.TCPServer(("", port), ServerHandler)
    listenThread = threading.Thread(target=httpd.serve_forever)
    listenThread.daemon = True
    listenThread.start()
    time.sleep(0.5)

class StateManager():
    partnerJobNum = 0
    partnerThrottling = 0
    delay = 0
    bandwidth = 0
    def setPartnerVariables(self, partnerObj):
        self.partnerJobNum = partnerObj['jobNum']
        self.partnerThrottling = partnerObj['throttling']
    # return jobNum and throttling as an object
    def stateToObj(self):
        #return {'jobNum': len(jobList), 'throttling': throttling, 'delay': self.delay, 'bandwidth': self.bandwidth, 'workerThreadNum': len(workThreadQueue)}
        return {'jobNum': len(jobList), 'throttling': throttling, 'delay': self.delay, 'bandwidth': self.bandwidth}
    def startStateSendingThread(self):
        stateSendingThread = threading.Thread(target=self.sendState)
        stateSendingThread.daemon = True
        stateSendingThread.start()
    def sendState(self):
        while True:
            try:
                if phase == EXECUTION or phase == AGGREGATION:
                    if isLocal:
                        requests.post(remote, data={'state': json.dumps(self.stateToObj())})
                    else:
                        requests.post(local, data={'state': json.dumps(self.stateToObj())})
            except:
                break
            time.sleep(STATETRANSFERPERIOD)

class HardwareMonitor():
    CPUUtilization = 0
    def startHardwareThread(self):
        hardwareThread = threading.Thread(target=self.updateCPUUtilization)
        hardwareThread.daemon = True
        hardwareThread.start()
    def updateCPUUtilization(self):
        while True:
            if isLocal:
                stateManager.delay = ping.verbose_ping(remote_addr, 'delay')
                stateManager.bandwidth = ping.verbose_ping(remote_addr, 'bandwidth')
            else:
                stateManager.delay = ping.verbose_ping(local_addr, 'delay')
                stateManager.bandwidth = ping.verbose_ping(local_addr, 'bandwidth')
            self.CPUUtilization = psutil.cpu_percent(interval=CPUUPDATEPERIOD)

class Adaptor():
    isAdaptorTransferThreadStart = False
    def startAdaptorTransferThread(self):
        self.isAdaptorTransferThreadStart = True
        adaptorTransferThread = threading.Thread(target=self._transferJobIfNeeded)
        adaptorTransferThread.daemon = True
        adaptorTransferThread.start()
    def _transferJobIfNeeded(self):
        while True:
            if self.shouldTransfer():
                jobListLock.acquire()
                tmpJob = jobList.pop()
                jobListLock.release()
                transferManager.transferJobs(tmpJob)
            time.sleep(JOBTRANSFERCHECKPERIOD)
    def shouldTransfer(self):
        if stateManager.delay > DELAYTHRESHOLD:
            return False
        if len(jobList) > stateManager.partnerJobNum + 1:
            return True
        elif len(jobList) == stateManager.partnerJobNum + 1:
            return throttling < stateManager.partnerThrottling
        return False

def transferResults(result):
    requests.post(local, data={'result': zlib.compress(json.dumps(result.toObj()))})

def getJobSeparationArray():
    result = numpy.array([allocateSize // minJobNum] * minJobNum)
    result[:allocateSize % minJobNum] += 1
    return result

def _initJobToJobList():
    index = 0
    jobID = 0
    for jobNum in getJobSeparationArray():
        jobListLock.acquire()
        jobList.append(MP4Job(jobID, index, [1.111111] * jobNum))
        jobListLock.release()
        jobID += 1
        index += jobNum

def _bootStrapTransferHalfJobs():
    for i in range(len(jobList) // 2):
        jobListLock.acquire()
        tmpJob = jobList.pop()
        jobListLock.release()
        transferManager.transferJobs(tmpJob)
    requests.post(remote, data={'signal': EXECUTION})

def aggregationTransferResults():
    while resultList:
        #print len(resultList)
        transferResults(resultList.pop())
    requests.post(local, data={'signal': TERMINATION})

def doJob(job):
    #print "=========="
    #print len(jobList)
    #print job.ID
    oldTime = datetime.now()
    for i in range(len(job.A)):
        for j in range(JOBITERATION):
            job.A[i] += 1.111111
    resultList.append(job)
    return (datetime.now() - oldTime).total_seconds()

def setResultA():
    while resultList:
        tmp = resultList.pop()
        for i in range(len(tmp.A)):
            resultA[tmp.startPoint + i] = tmp.A[i]

transferManager = TransferManager()
stateManager = StateManager()
hardwareMonitor = HardwareMonitor()
adaptor = Adaptor()

if __name__ == '__main__':
    # set configuration
    #isLocal = sys.argv[1] == 'local'
    #throttling = float(sys.argv[2])
    startListenThread()
    stateManager.startStateSendingThread()
    hardwareMonitor.startHardwareThread()

    if isLocal:
        # Initialize jobs
        _initJobToJobList()
        # Bootstrap phase
        _bootStrapTransferHalfJobs()
        # enter EXECUTION phase
        phase = EXECUTION

    while True:
        if phase == BOOTSTRAP:
            #print "BOOTSTRAP"
            time.sleep(0.1)
        elif phase == EXECUTION:
            #print "EXECUTION"
            if not adaptor.isAdaptorTransferThreadStart:
                adaptor.startAdaptorTransferThread()
            jobListLock.acquire()
            tmpJob = jobList.pop()
            jobListLock.release()
            time.sleep(doJob(tmpJob) * (1 - throttling) / throttling)
            if not jobList:
                phase = AGGREGATION
        elif phase == AGGREGATION:
            print "AGGREGATION"
            if isLocal:
                if isRemoteTerminate:
                    phase = TERMINATION
                time.sleep(0.1)
            else:
                aggregationTransferResults()
                phase = TERMINATION
        elif phase == TERMINATION:
            print "TERMINATION"
            print len(resultList)
            if isLocal:
                setResultA()
                print '================'
                print len(resultA)
                print resultA[:10]
                print all(x == resultA[0] for x in resultA)
                print '================'
            time.sleep(3)
            break
