importScripts('lepton-scalar.js')

/**
 * onmessage receives some data that looks like this:
 * { 'type': 'compress|decompress', data: [], 'id': 'str'}
 *
 * It sends back data like this:
 * { 'id': 'str', 'data': []}
 */
onmessage = function(e) {
    lepton_callback = function(result) {
        postMessage({
            data: result,
            id: e.data['id']
        });
    };

    if(e.data['type'] == 'compress') {
        Lepton.compress(e.data['data'], lepton_callback);
    }
    else if(e.data['type'] == 'decompress') {
        Lepton.decompress(e.data['data'], lepton_callback);
    }
}
