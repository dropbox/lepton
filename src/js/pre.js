var Lepton = {
    run: function(data, cb) {
        var result = [];
        var inputIndex = 0;
        var cb = cb; // callback fires onExit and returns status and result
        var Module = {
            arguments: [],
            postRun: function() {
                cb(result);
            },
            stdin: function() {
                if(inputIndex < data.length) {
                    thingy = data[inputIndex++];
                    return thingy;
                }
                else {
                    return null;
                }
            },
            stdout: function(x) {
                if(x !== null) {
                    result.push(x);
                }
            }
        };
