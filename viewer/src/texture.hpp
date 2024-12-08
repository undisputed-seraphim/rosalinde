#pragma once

class Texture2D {
	unsigned _hnd;
	unsigned _w, _h;

public:
	struct Image {
		unsigned width, height;
		const void* pixels = NULL;
	};

	Texture2D();
	Texture2D(const Texture2D&) = delete;
	Texture2D(Texture2D&&) noexcept;
	~Texture2D() noexcept;

	// generates texture from image data
	void Init(Image);
	void Subimage(Image, unsigned xoffset, unsigned yoffset);

	const Texture2D& Active(unsigned samplerID) const noexcept;
	const Texture2D& Bind() const noexcept;
	const Texture2D& Unbind() const noexcept;

	explicit operator unsigned() const noexcept;
	explicit operator bool() const noexcept;
};
