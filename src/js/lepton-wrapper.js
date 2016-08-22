function Lepton(maxWorkers) {
    this.maxWorkers = maxWorkers;
    this.workerCount = 0;
    this.jobCount = 0;  // increasing integer for creating unique callbacks
    this.workers = {
        free: [],
        busy: []
    };

    this.callbacks = {}; // link a callback id to a list of [func, worker]
}

/**
 * Handle all webworker callbacks and dispatch job calbacks to the proper
 * functions.
 */
Lepton.prototype._callbackHandler = function(e) {
    cb_data = this.callbacks[e.data['id']];
    cb_data[0](e.data['data']);
    worker_index = this.workers['busy'].indexOf(cb_data[1]);
    if(worker_index != -1) {
        this.workers['busy'].splice(worker_index, 1);
        this.workers['free'].push(cb_data[1]);
    }
    delete this.callbacks[e.data['id']];
}

/**
 * Manage the pool of webworkers. If we don't have enough create a new one,
 * if we have reached the max, return undefined
 */
Lepton.prototype._grabWorker = function() {
    worker = this.workers['free'].pop();
    if(worker == undefined) {
        if(this.workerCount < this.maxWorkers) {
            this.workerCount++;
            worker = new Worker("lepton-worker.js");
            var obj = this;
            worker.onmessage = function(e) { obj._callbackHandler(e) };
            console.log("Creating Worker #" + this.workerCount);
        }
        else {
            console.log("No Free Workers");
            return undefined;
        }
    }
    this.workers['busy'].push(worker);
    return worker;
}

/**
 * Internal helper function which takes some data (lepton or jpeg)
 * the operation type, an ID, and a callback. This data is used to
 * start a lepton job on a webworker.
 */
Lepton.prototype._doWork = function(arr, type, id, cb) {
    worker = this._grabWorker();
    if(worker == undefined) {
        return undefined;
    }
    this.callbacks[id] = [cb, worker];
    msg = {
        'type': type,
        'id': id,
        'data': arr
    };
    worker.postMessage(msg);
    return msg;
}

/**
 * Take an array of jpeg data and compress it. An array of
 * compressed lepton data will be passed to the callback function.
 */
Lepton.prototype.compressArray = function(arr, cb) {
    id = this.jobCount++;
    return this._doWork(arr, 'compress', id, cb);
}

/**
 * Take an array of lepton compressed data and decompress it. An array of
 * decompressed data will be passed to the callback function.
 */
Lepton.prototype.decompressArray = function(arr, cb) {
    id = this.jobCount++;
    return this._doWork(arr, 'decompress', id, cb);
}

/**
 * Compress the data from a given URL
 */
Lepton.prototype.compressURL = function(url, cb) {
    request = new XMLHttpRequest();
    request.responseType = "arraybuffer";
    var lepObj = this;
    request.onload = function(e) {
        lepObj.compressArray(new Uint8Array(request.response), cb);
    }
    request.open("GET", url, true);
    request.send();
}

/**
 * Decompress the data from a given URL
 */
Lepton.prototype.decompressURL = function(url, cb) {
    request = new XMLHttpRequest();
    request.responseType = "arraybuffer";
    var lepObj = this;
    request.onload = function(e) {
        lepObj.decompressArray(new Uint8Array(request.response), cb);
    }
    request.open("GET", url, true);
    request.send();
}
