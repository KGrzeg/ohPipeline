cmake_minimum_required(VERSION 3.4.1)
project(ohPipeline LANGUAGES CXX C)

if(NOT DEFINED OHNET_PATH)
  message(FATAL_ERROR "You need to provide -DOHNET_PATH that points to built headers")
endif()

set(ENDIANNESS DEFINE_LITTLE_ENDIAN)

set(sources
  OpenHome/Media/Pipeline/VolumeRamper.cpp
  OpenHome/Media/Pipeline/AudioDumper.cpp
  OpenHome/Media/Pipeline/AudioReservoir.cpp
  OpenHome/Media/Pipeline/DecodedAudioAggregator.cpp
  OpenHome/Media/Pipeline/DecodedAudioReservoir.cpp
  OpenHome/Media/Pipeline/DecodedAudioValidator.cpp
  OpenHome/Media/Pipeline/Drainer.cpp
  OpenHome/Media/Pipeline/EncodedAudioReservoir.cpp
  OpenHome/Media/Pipeline/Flusher.cpp
  OpenHome/Media/Pipeline/Logger.cpp
  OpenHome/Media/Pipeline/Msg.cpp
  OpenHome/Media/Pipeline/Muter.cpp
  OpenHome/Media/Pipeline/MuterVolume.cpp
  OpenHome/Media/Pipeline/PreDriver.cpp
  OpenHome/Media/Pipeline/Attenuator.cpp
  OpenHome/Media/Pipeline/Ramper.cpp
  OpenHome/Media/Pipeline/Reporter.cpp
  OpenHome/Media/Pipeline/AirplayReporter.cpp
  OpenHome/Media/Pipeline/SpotifyReporter.cpp
  OpenHome/Media/Pipeline/RampValidator.cpp
  OpenHome/Media/Pipeline/Rewinder.cpp
  OpenHome/Media/Pipeline/Router.cpp
  OpenHome/Media/Pipeline/StreamValidator.cpp
  OpenHome/Media/Pipeline/Seeker.cpp
  OpenHome/Media/Pipeline/PhaseAdjuster.cpp
  OpenHome/Media/Pipeline/Skipper.cpp
  OpenHome/Media/Pipeline/StarvationRamper.cpp
  OpenHome/Media/Pipeline/Stopper.cpp
  OpenHome/Media/Pipeline/TrackInspector.cpp
  OpenHome/Media/Pipeline/VariableDelay.cpp
  OpenHome/Media/Pipeline/Waiter.cpp
  OpenHome/Media/Pipeline/Pipeline.cpp
  OpenHome/Media/Pipeline/ElementObserver.cpp
  OpenHome/Media/ClockPuller.cpp
  OpenHome/Media/IdManager.cpp
  OpenHome/Media/Filler.cpp
  OpenHome/Media/Supply.cpp
  OpenHome/Media/SupplyAggregator.cpp
  OpenHome/Media/Utils/AnimatorBasic.cpp
  OpenHome/Media/Utils/ProcessorAudioUtils.cpp
  OpenHome/Media/Utils/ClockPullerManual.cpp
  OpenHome/Media/Codec/Mpeg4.cpp
  OpenHome/Media/Codec/Container.cpp
  OpenHome/Media/Codec/Id3v2.cpp
  OpenHome/Media/Codec/MpegTs.cpp
  OpenHome/Media/Codec/CodecController.cpp
  OpenHome/Media/Protocol/Protocol.cpp
  OpenHome/Media/Protocol/ProtocolHls.cpp
  OpenHome/Media/Protocol/ProtocolHttp.cpp
  OpenHome/Media/Protocol/ProtocolFile.cpp
  OpenHome/Media/Protocol/ProtocolTone.cpp
  OpenHome/Media/Protocol/Icy.cpp
  OpenHome/Media/Protocol/Rtsp.cpp
  OpenHome/Media/Protocol/ProtocolRtsp.cpp
  OpenHome/Media/Protocol/ContentAudio.cpp
  OpenHome/Media/UriProviderRepeater.cpp
  OpenHome/Media/UriProviderSingleTrack.cpp
  OpenHome/Media/PipelineManager.cpp
  OpenHome/Media/PipelineObserver.cpp
  OpenHome/Media/MuteManager.cpp
  OpenHome/Media/FlywheelRamper.cpp
  OpenHome/Media/MimeTypeList.cpp
  OpenHome/Media/Utils/AllocatorInfoLogger.cpp
  OpenHome/Configuration/ConfigManager.cpp
  OpenHome/Media/Utils/Silencer.cpp
  OpenHome/SocketHttp.cpp
  OpenHome/SocketSsl.cpp
)

add_library(ohPipeline SHARED ${sources})
target_include_directories(ohPipeline PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(ohPipeline PRIVATE ${OHNET_PATH})
target_compile_definitions(ohPipeline PUBLIC ${ENDIANNESS})
target_compile_definitions(ohPipeline PUBLIC _GNU_SOURCE)
