#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <span>
#include <string>

namespace criware {

class AudioSource {
public:
	virtual ~AudioSource() = default;
	virtual size_t read(uint8_t* buf, size_t len) = 0;
	virtual size_t total_size() const = 0;
	virtual bool seekable() const { return false; }
	virtual void seek(size_t) {}
};

class SpanAudioSource final : public AudioSource {
public:
	explicit SpanAudioSource(std::span<const uint8_t> data)
		: _data(data), _cursor(0) {}

	size_t read(uint8_t* buf, size_t len) override {
		size_t avail = _data.size() - _cursor;
		size_t n = len < avail ? len : avail;
		if (n == 0) return 0;
		std::memcpy(buf, _data.data() + _cursor, n);
		_cursor += n;
		return n;
	}

	size_t total_size() const override { return _data.size(); }
	bool seekable() const override { return true; }
	void seek(size_t offset) override { _cursor = offset < _data.size() ? offset : _data.size(); }

private:
	std::span<const uint8_t> _data;
	size_t _cursor;
};

class FileAudioSource final : public AudioSource {
public:
	explicit FileAudioSource(const std::string& path)
		: _file(path, std::ios::binary) {
		if (_file) {
			_file.seekg(0, std::ios::end);
			_size = static_cast<size_t>(_file.tellg());
			_file.seekg(0, std::ios::beg);
		}
	}

	size_t read(uint8_t* buf, size_t len) override {
		if (!_file) return 0;
		_file.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(len));
		size_t got = static_cast<size_t>(_file.gcount());
		_cursor += got;
		return got;
	}

	size_t total_size() const override { return _size; }
	bool seekable() const override { return true; }
	void seek(size_t offset) override {
		_file.clear();
		_file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
		_cursor = offset;
	}

private:
	std::ifstream _file;
	size_t _size = 0;
	size_t _cursor = 0;
};

} // namespace criware
