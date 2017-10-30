#!/usr/bin/env python
"""TestConnectionManagerService - test UPnP AV ConnectionManager aervice

Parameters:
    arg#1 - MediaRenderer DUT ['local' for internal SoftPlayer on loopback]

Checks (what little) functionality offered by ConnectionManager service
"""
import _Paths   # NOQA
import CommonConnectionManagerService as BASE
import _ProtocolInfo as ProtocolInfo
import sys


class TestConnectionManagerService( BASE.CommonConnectionManagerService ):

    def __init__( self ):
        BASE.CommonConnectionManagerService.__init__( self )
        self.doc         = __doc__
        self.sourceProto = []
        self.sinkProto   = ProtocolInfo.ProtocolInfo()


if __name__ == '__main__':

    BASE.Run( sys.argv )
