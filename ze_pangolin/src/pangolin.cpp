// Copyright (c) 2015-2016, ETH Zurich, Wyss Zurich, Zurich Eye
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the ETH Zurich, Wyss Zurich, Zurich Eye nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL ETH Zurich, Wyss Zurich, Zurich Eye BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <ze/pangolin/pangolin.hpp>

namespace ze {

PangolinPlotter::PangolinPlotter(const std::string& window_title,
                                 int width,
                                 int height)
  : window_title_(window_title)
  , width_(width)
  , height_(height)
{
  // Thread out a pangolin loop.
  thread_.reset(new std::thread(&PangolinPlotter::loop, this));
}

PangolinPlotter::~PangolinPlotter()
{
  requestStop();
  thread_->join();
}

PangolinPlotter& PangolinPlotter::instance(
    const std::string& window_title,
    int width,
    int height)
{
  // Although this does not appear to be thread safe, it is as of cpp11.
  static PangolinPlotter instance(window_title, width, height);
  return instance;
}

void PangolinPlotter::loop()
{
  // Create OpenGL window and switch to context.
  pangolin::CreateWindowAndBind(window_title_, width_, height_);

  while(!isStopRequested())
  {
    // If the addition of a logger was requested, process the request.
    if (add_logger_)
    {
      data_logs_[new_logger_identifier_] = std::make_shared<pangolin::DataLog>();

      // Set the labels before adding the data_log to the plotter
      std::vector<std::string> labels = {new_logger_identifier_};
      data_logs_.at(new_logger_identifier_)->SetLabels(labels);

      plotters_[new_logger_identifier_] = std::make_shared<pangolin::Plotter>(
                                data_logs_.at(new_logger_identifier_).get());
      // plotter.SetBounds(0.0, 1.0, 0.0, 1.0);
      plotters_[new_logger_identifier_]->Track("$i");

      // Add the new plotter to the pangolin window.
      pangolin::Display("multi")
          .SetLayout(pangolin::LayoutEqual)
          .AddDisplay(*plotters_[new_logger_identifier_]);

      VLOG(3) << "Add plotter display for: " << new_logger_identifier_;

      add_logger_ = false;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Swap frames and Process Events
    pangolin::FinishFrame();

    std::this_thread::sleep_for(std::chrono::milliseconds(thread_sleep_ms_));
  }
}

std::shared_ptr<pangolin::DataLog>& PangolinPlotter::getLoggerOrCreate(
    const std::string& identifier)
{
  // As the visualization runs in a separate thread, the gl context is not accessible
  // from within this function. We request the thread to add a new plotter and
  // wait for it to be ready.
  if (data_logs_.find(identifier) == data_logs_.end())
  {
    std::lock_guard<std::mutex> lock(add_logger_mutex_);

    add_logger_ = true;
    new_logger_identifier_ = identifier;

    while(add_logger_)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(thread_sleep_ms_));
    }
    CHECK(data_logs_.find(identifier) != data_logs_.end())
        << "A pangolin plotter (" << identifier << ") that was requested to be added and confirmed"
           "does actually not exist.";
  }

  return data_logs_.at(identifier);
}

void PangolinPlotter::requestStop()
{
  std::lock_guard<std::mutex> lock(stop_mutex_);
  stop_requested_ = true;
}

bool PangolinPlotter::isStopRequested()
{
   std::lock_guard<std::mutex> lock(stop_mutex_);
   return stop_requested_;
}

} // namespace ze
