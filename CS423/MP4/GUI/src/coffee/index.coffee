stateUpdatePeriod = 1000
jobTransferUpdatePeriod = 1000
jobTransfered =
    'local': 0,
    'remote': 0

updateThrottling = (dest) ->
    val = $('#' + dest + ' input').val()
    if val.length > 0 and (not isNaN val) and val > 0 and val < 1
        myRequest dest, 'throttling/' + val
    else
        alert('please enter a floating point number between 0 and 1')
    $('#' + dest + ' input').val('')

myRequest = (dest, query) ->
    $.getJSON '/' + dest + '/' + query, (elem) ->
        if query is 'state'
            $('#' + dest + ' .job').text(elem.jobNum)
            $('#' + dest + ' .finished').text(elem.resultNum)
            $('#' + dest + ' .throttling').text(elem.throttling)
            $('#' + dest + ' .delay').text(elem.delay)
            $('#' + dest + ' .bandwidth').text(elem.bandwidth)
        else if query is 'jobTransfered'
            jobTransfered[dest] += elem.jobTransfered
            $('#' + dest + ' .jobTransfered').text(jobTransfered[dest])

window.onload = ->
    setInterval ->
        myRequest 'local', 'jobTransfered'
        myRequest 'remote', 'jobTransfered'
    , jobTransferUpdatePeriod
    setInterval ->
        myRequest 'local', 'state'
        myRequest 'remote', 'state'
    , stateUpdatePeriod
