// SPDX-FileCopyrightText: 2021 Sveriges Television AB
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ui/VivictUI.hh"
#include "logging/Logging.hh"
#include "VideoMetadata.hh"
#include "sdl/SDLAudioOutput.hh"

VideoMetadata* metadataPtr(const std::vector<VideoMetadata> &v) {
  if (v.empty()) {
    return nullptr;
  }
  return new VideoMetadata(v[0]);
}


vivictpp::ui::VivictUI::VivictUI(VivictPPConfig vivictPPConfig)
  : eventLoop(),
    vivictPP(vivictPPConfig, eventLoop, vivictpp::sdl::audioOutputFactory),
    screenOutput(metadataPtr(vivictPP.getVideoInputs().metadata()[0]),
                 metadataPtr(vivictPP.getVideoInputs().metadata()[1]),
                 vivictPPConfig.sourceConfigs),
    splitScreenDisabled(vivictPPConfig.sourceConfigs.size() == 1),
    logger(vivictpp::logging::getOrCreateLogger("VivictUI")) {
  displayState.splitScreenDisabled = splitScreenDisabled;

}

int vivictpp::ui::VivictUI::run() {
  eventLoop.scheduleAdvanceFrame(5);
  eventLoop.start(*this);
  logger->debug("vivictpp::ui::VivictUI::run exit");
  return 0;
}

void vivictpp::ui::VivictUI::advanceFrame() {
  vivictPP.advanceFrame();
}

void vivictpp::ui::VivictUI::refreshDisplay() {
  logger->trace("vivictpp::ui::VivictUI::refreshDisplay");
  std::array<vivictpp::libav::Frame, 2> frames = vivictPP.getVideoInputs().firstFrames();
  if (displayState.displayTime) {
    displayState.timeStr = vivictpp::util::formatTime(vivictPP.getPts());
  }
  displayState.pts = vivictPP.getPts();
  screenOutput.displayFrame(frames, displayState);
}


void vivictpp::ui::VivictUI::queueAudio() {
  vivictPP.queueAudio();
}

void vivictpp::ui::VivictUI::mouseDragStart() {
  screenOutput.setCursorHand();
}

void vivictpp::ui::VivictUI::mouseDragEnd() {
  screenOutput.setCursorDefault();
}

void vivictpp::ui::VivictUI::mouseDrag(int xrel, int yrel) {
  displayState.panX -=
    (float)xrel / displayState.zoom.multiplier();
  displayState.panY -=
    (float)yrel / displayState.zoom.multiplier();
  eventLoop.scheduleRefreshDisplay(0);
}

void vivictpp::ui::VivictUI::mouseMotion(int x, int y) {
  (void) y;
  displayState.splitPercent =
    x * 100.0 / screenOutput.getWidth();
  eventLoop.scheduleRefreshDisplay(0);
}

void vivictpp::ui::VivictUI::mouseWheel(int x, int y) {
  displayState.panX -=
    (float)10 * x / displayState.zoom.multiplier();
  displayState.panY -=
    (float)10 * y / displayState.zoom.multiplier();
  eventLoop.scheduleRefreshDisplay(0);
}

void vivictpp::ui::VivictUI::mouseClick(int x, int y) {
  if (displayState.displayPlot && y > screenOutput.getHeight() * 0.7) {
    float seekRel = x / (float) screenOutput.getWidth();
    VideoMetadata &metadata = vivictPP.getVideoInputs().metadata()[0][0];
    float pos = metadata.startTime + metadata.duration * seekRel;
    vivictPP.seek(pos);
    logger->debug("seeking to {}", pos);
  } else {
    togglePlaying();
  }
}

void vivictpp::ui::VivictUI::togglePlaying() {
  displayState.isPlaying = vivictPP.togglePlaying() == PlaybackState::PLAYING;
}

void vivictpp::ui::VivictUI::onQuit() {
  eventLoop.stop();
  vivictPP.onQuit();
}

void vivictpp::ui::VivictUI::keyPressed(std::string key) {
  logger->debug("vivictpp::ui::VivictUI::keyPressed key='{}'", key);
  if (key.length() == 1) {
    switch (key[0]) {
    case 'Q':
      onQuit();
      break;
    case '.':
      vivictPP.seekNextFrame();
      break;
    case ',':
      vivictPP.seekPreviousFrame();
      break;
    case '/':
      vivictPP.seekRelative(5);
      break;
    case 'M':
      vivictPP.seekRelative(-5);
      break;
    case 'U':
      displayState.zoom.increment();
      eventLoop.scheduleRefreshDisplay(0);
      logger->debug("Zoom: {}", displayState.zoom.get());
      break;
    case 'I':
      displayState.zoom.decrement();
      eventLoop.scheduleRefreshDisplay(0);
      logger->debug("Zoom: {}", displayState.zoom.get());
      break;
    case '0':
      displayState.zoom.set(0);
      displayState.panX = 0;
      displayState.panY = 0;
      eventLoop.scheduleRefreshDisplay(0);
      break;
    case 'F':
      displayState.fullscreen = !displayState.fullscreen;
      screenOutput.setFullscreen(displayState.fullscreen);
      break;
    case 'T':
      displayState.displayTime = !displayState.displayTime;
      eventLoop.scheduleRefreshDisplay(0);
      break;
    case 'D':
      displayState.displayMetadata = !displayState.displayMetadata;
      eventLoop.scheduleRefreshDisplay(0);
      break;
    case 'P':
      displayState.displayPlot = !displayState.displayPlot;
      eventLoop.scheduleRefreshDisplay(0);
      break;
    case '1':
      vivictPP.switchStream(-1);
      break;
    case '2':
      vivictPP.switchStream(1);
      break;
    }
  } else {
    if (key == "Space") {
      togglePlaying();
    }
  }
}
