"""_ProductData - product specific data
"""

# Available source specified by type
ds =   {'NetAux'  : 'Net Aux',
        'Playlist': 'Playlist',
        'Radio'   : 'Radio',
        'Receiver': 'Songcast',
        'Spotify' : 'Spotify',
        'UpnpAv'  : 'UPnP AV',
        'Scd'     : 'Scd' }

kSrcByType = { 'SoftPlayer' : ds,
               'Unknown'    : ds }
