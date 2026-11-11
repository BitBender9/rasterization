#include "rasterizer.hpp"

#include "uselibpng.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace rast
{
namespace
{

Value Subtract(const Value& left, const Value& right)
{
  Value result{};
  for (std::size_t i = 0; i < result.size(); ++i)
  {
    result[i] = left[i] - right[i];
  }
  return result;
}

Value Add(const Value& left, const Value& right)
{
  Value result{};
  for (std::size_t i = 0; i < result.size(); ++i)
  {
    result[i] = left[i] + right[i];
  }
  return result;
}

Value Multiply(const Value& value, double scalar)
{
  Value result{};
  for (std::size_t i = 0; i < result.size(); ++i)
  {
    result[i] = value[i] * scalar;
  }
  return result;
}

Value Divide(const Value& value, double scalar)
{
  return Multiply(value, 1.0 / scalar);
}

std::uint8_t ToByte(double channel)
{
  const double clamped = std::clamp(channel, 0.0, 1.0);
  return static_cast<std::uint8_t>(std::lround(clamped * 255.0));
}

std::runtime_error LineError(std::size_t lineNumber,
                             const std::string& message)
{
  return std::runtime_error("line " + std::to_string(lineNumber) + ": " +
                            message);
}

void EnsureNoExtra(std::istringstream& input, std::size_t lineNumber)
{
  std::string extra;
  if (input >> extra)
  {
    throw LineError(lineNumber, "unexpected value '" + extra + "'");
  }
}

std::vector<double> ReadValues(std::istringstream& input,
                               std::size_t lineNumber)
{
  std::vector<double> values;
  double value = 0.0;
  while (input >> value)
  {
    if (!std::isfinite(value))
    {
      throw LineError(lineNumber, "attribute values must be finite");
    }
    values.push_back(value);
  }
  if (!input.eof())
  {
    throw LineError(lineNumber, "attribute list contains a non-number");
  }
  return values;
}

}  // namespace

std::vector<Value> DDA(Value first, Value second, std::size_t dimension)
{
  if (dimension >= kValueSize)
  {
    throw std::invalid_argument("DDA dimension is out of range");
  }
  if (first[dimension] > second[dimension])
  {
    std::swap(first, second);
  }

  const double distance = second[dimension] - first[dimension];
  if (distance == 0.0)
  {
    return {};
  }

  const Value step = Divide(Subtract(second, first), distance);
  const double firstSample = std::ceil(first[dimension]);
  const double sampleCount = std::ceil(second[dimension]) - firstSample;
  if (sampleCount <= 0.0 ||
      sampleCount > static_cast<double>(
                        std::numeric_limits<std::size_t>::max()))
  {
    return {};
  }

  Value current =
      Add(first, Multiply(step, firstSample - first[dimension]));
  std::vector<Value> samples;
  samples.reserve(static_cast<std::size_t>(sampleCount));
  for (std::size_t i = 0; i < static_cast<std::size_t>(sampleCount); ++i)
  {
    samples.push_back(current);
    current = Add(current, step);
  }
  return samples;
}

void Rasterizer::ConfigureOutput(unsigned width, unsigned height,
                                 std::string filename)
{
  if (width == 0 || height == 0)
  {
    throw std::invalid_argument("PNG dimensions must be positive");
  }
  if (filename.empty())
  {
    throw std::invalid_argument("PNG filename must not be empty");
  }
  if (static_cast<std::size_t>(width) >
      std::numeric_limits<std::size_t>::max() /
          static_cast<std::size_t>(height))
  {
    throw std::overflow_error("PNG dimensions are too large");
  }
  const std::size_t pixelCount =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  if (pixelCount > std::numeric_limits<std::size_t>::max() / 4)
  {
    throw std::overflow_error("PNG dimensions are too large");
  }

  width_ = width;
  height_ = height;
  outputFilename_ = std::move(filename);
  pixels_.assign(pixelCount * 4, 0);
}

void Rasterizer::SetPositions(unsigned size,
                              const std::vector<double>& values)
{
  if (size != 4)
  {
    throw std::invalid_argument("core position size must be 4");
  }
  if (values.empty() || values.size() % size != 0)
  {
    throw std::invalid_argument(
        "position requires a non-empty multiple of 4 values");
  }

  positions_.clear();
  positions_.reserve(values.size() / size);
  for (std::size_t i = 0; i < values.size(); i += size)
  {
    positions_.push_back(
        {values[i], values[i + 1], values[i + 2], values[i + 3]});
  }
}

void Rasterizer::SetColors(unsigned size, const std::vector<double>& values)
{
  if (size != 3)
  {
    throw std::invalid_argument("core color size must be 3");
  }
  if (values.empty() || values.size() % size != 0)
  {
    throw std::invalid_argument(
        "color requires a non-empty multiple of 3 values");
  }

  colors_.clear();
  colors_.reserve(values.size() / size);
  for (std::size_t i = 0; i < values.size(); i += size)
  {
    colors_.push_back({values[i], values[i + 1], values[i + 2], 1.0});
  }
}

void Rasterizer::RequireConfigured() const
{
  if (width_ == 0 || height_ == 0 || pixels_.empty())
  {
    throw std::logic_error("png must be configured before drawing");
  }
}

Value Rasterizer::ProcessVertex(std::size_t index) const
{
  if (index >= positions_.size() || index >= colors_.size())
  {
    throw std::out_of_range("draw references missing position or color data");
  }

  const Position& position = positions_[index];
  if (!std::isfinite(position[3]) || position[3] == 0.0)
  {
    throw std::runtime_error("position w must be finite and nonzero");
  }
  const double inverseW = 1.0 / position[3];
  const Color& color = colors_[index];
  return {
      (position[0] / position[3] + 1.0) * static_cast<double>(width_) / 2.0,
      (position[1] / position[3] + 1.0) * static_cast<double>(height_) / 2.0,
      position[2] / position[3],
      inverseW,
      color[0],
      color[1],
      color[2],
      color[3],
  };
}

void Rasterizer::DrawArraysTriangles(std::size_t first, std::size_t count)
{
  RequireConfigured();
  if (count % 3 != 0)
  {
    throw std::invalid_argument(
        "drawArraysTriangles count must be a multiple of 3");
  }
  if (first > positions_.size() || count > positions_.size() - first ||
      first > colors_.size() || count > colors_.size() - first)
  {
    throw std::out_of_range(
        "drawArraysTriangles range exceeds position or color data");
  }

  for (std::size_t i = first; i < first + count; i += 3)
  {
    DrawTriangle(ProcessVertex(i), ProcessVertex(i + 1),
                 ProcessVertex(i + 2));
  }
}

void Rasterizer::DrawTriangle(Value first, Value second, Value third)
{
  std::array<Value, 3> vertices{first, second, third};
  std::stable_sort(vertices.begin(), vertices.end(),
                   [](const Value& left, const Value& right)
                   {
                     return left[Y] < right[Y];
                   });

  const std::vector<Value> longEdge = DDA(vertices[0], vertices[2], Y);
  std::vector<Value> shortEdges = DDA(vertices[0], vertices[1], Y);
  std::vector<Value> lowerShortEdge = DDA(vertices[1], vertices[2], Y);
  shortEdges.insert(shortEdges.end(), lowerShortEdge.begin(),
                    lowerShortEdge.end());

  const std::size_t scanlineCount =
      std::min(longEdge.size(), shortEdges.size());
  for (std::size_t i = 0; i < scanlineCount; ++i)
  {
    for (const Value& sample : DDA(longEdge[i], shortEdges[i], X))
    {
      WritePixel(sample);
    }
  }
}

void Rasterizer::WritePixel(const Value& value)
{
  const long x = std::lround(value[X]);
  const long y = std::lround(value[Y]);
  if (x < 0 || y < 0 || x >= static_cast<long>(width_) ||
      y >= static_cast<long>(height_))
  {
    return;
  }

  const std::size_t offset =
      (static_cast<std::size_t>(y) * width_ + static_cast<std::size_t>(x)) *
      4;
  pixels_[offset] = ToByte(value[R]);
  pixels_[offset + 1] = ToByte(value[G]);
  pixels_[offset + 2] = ToByte(value[B]);
  pixels_[offset + 3] = ToByte(value[A]);
}

void Rasterizer::Save() const
{
  RequireConfigured();
  Image image(width_, height_);
  for (unsigned y = 0; y < height_; ++y)
  {
    for (unsigned x = 0; x < width_; ++x)
    {
      const std::size_t offset =
          (static_cast<std::size_t>(y) * width_ + x) * 4;
      image[y][x].red = pixels_[offset];
      image[y][x].green = pixels_[offset + 1];
      image[y][x].blue = pixels_[offset + 2];
      image[y][x].alpha = pixels_[offset + 3];
    }
  }
  image.save(outputFilename_.c_str());
}

Rasterizer RenderFile(const std::string& inputFilename)
{
  std::ifstream input(inputFilename);
  if (!input)
  {
    throw std::runtime_error("could not open input file '" + inputFilename +
                             "'");
  }

  Rasterizer rasterizer;
  bool sawCommand = false;
  bool configured = false;
  std::string line;
  std::size_t lineNumber = 0;
  while (std::getline(input, line))
  {
    ++lineNumber;
    const std::size_t comment = line.find('#');
    if (comment != std::string::npos)
    {
      line.erase(comment);
    }

    std::istringstream tokens(line);
    std::string command;
    if (!(tokens >> command))
    {
      continue;
    }
    if (!sawCommand && command != "png")
    {
      throw LineError(lineNumber, "png must be the first command");
    }
    sawCommand = true;

    try
    {
      if (command == "png")
      {
        if (configured)
        {
          throw std::invalid_argument("png may only appear once");
        }
        long long width = 0;
        long long height = 0;
        std::string filename;
        if (!(tokens >> width >> height >> filename) || width <= 0 ||
            height <= 0 ||
            width > std::numeric_limits<unsigned>::max() ||
            height > std::numeric_limits<unsigned>::max())
        {
          throw std::invalid_argument(
              "png requires positive unsigned width and height, and filename");
        }
        EnsureNoExtra(tokens, lineNumber);
        rasterizer.ConfigureOutput(static_cast<unsigned>(width),
                                   static_cast<unsigned>(height), filename);
        configured = true;
      }
      else if (command == "position" || command == "color")
      {
        unsigned size = 0;
        if (!(tokens >> size))
        {
          throw std::invalid_argument(command + " requires a size");
        }
        const std::vector<double> values = ReadValues(tokens, lineNumber);
        if (command == "position")
        {
          rasterizer.SetPositions(size, values);
        }
        else
        {
          rasterizer.SetColors(size, values);
        }
      }
      else if (command == "drawArraysTriangles")
      {
        long long first = -1;
        long long count = -1;
        if (!(tokens >> first >> count) || first < 0 || count < 0)
        {
          throw std::invalid_argument(
              "drawArraysTriangles requires non-negative first and count");
        }
        EnsureNoExtra(tokens, lineNumber);
        rasterizer.DrawArraysTriangles(static_cast<std::size_t>(first),
                                       static_cast<std::size_t>(count));
      }
      else
      {
        throw std::invalid_argument("unsupported command '" + command + "'");
      }
    }
    catch (const std::exception& error)
    {
      const std::string prefix = "line " + std::to_string(lineNumber) + ":";
      if (std::string(error.what()).rfind(prefix, 0) == 0)
      {
        throw;
      }
      throw LineError(lineNumber, error.what());
    }
  }

  if (!configured)
  {
    throw std::runtime_error("input file does not contain a png command");
  }
  rasterizer.Save();
  return rasterizer;
}

}  // namespace rast
