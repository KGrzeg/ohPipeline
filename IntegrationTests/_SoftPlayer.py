"""Utility to execute standalone TestMediaPlayer executable
"""
import _Paths   # NOQA
import CommonSoftPlayer as BASE
import Config
import os
import platform
import time

_platform = platform.system()
if _platform in ['Windows', 'cli']:
    kExe = os.path.join( '..', 'install', 'bin', 'TestMediaPlayer.exe' )
elif _platform in ['Linux', 'Darwin']:
    kExe = os.path.join( '..', 'install', 'bin', 'TestMediaPlayer' )


class SoftPlayer( BASE.CommonSoftPlayer ):

    def __init__( self, **kw ):
        """Constructor for Airplay Dropout test"""
        self.doc = __doc__
        odpPort  = BASE.GetOdpPort( **kw )
        BASE.CommonSoftPlayer.__init__( self, kExe, aOdpPort=odpPort, aCloudIndex='1', **kw )
        time.sleep( 5 )


if __name__ == '__main__':

    # start softplayer, wait for exit
    tuneinId = Config.Config().Get( 'tunein.partnerid' )
    tidalId  = Config.Config().Get( 'tidal.id' )
    qobuzId  = Config.Config().Get( 'qobuz.id' ) + ':' + Config.Config().Get( 'qobuz.secret' )
    s = SoftPlayer( aRoom='TestDev', aTuneInId=tuneinId, aTidalId=tidalId, aQobuzId=qobuzId, aCloudIndex='1' )
    if _platform in ['Windows', 'cli']:
        import msvcrt
        print( '\nPress ANY KEY to EXIT' )
        msvcrt.getch()
    else:
        import sys
        print( '\nPress ENTER to EXIT' )
        sys.stdin.readline()
    s.Shutdown()
    s.log.Cleanup()
