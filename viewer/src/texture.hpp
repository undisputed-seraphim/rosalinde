#pragma once

class Texture2D {
public:
	// holds the ID of the texture object, used for all texture operations to reference to this particular texture
	unsigned int ID;
	// texture image dimensions
	unsigned int Width, Height; // width and height of loaded image in pixels
	// texture Format
	unsigned int Internal_Format; // format of texture object
	unsigned int Image_Format;	  // format of loaded image
	// texture configuration
	unsigned int Wrap_S;	 // wrapping mode on S axis
	unsigned int Wrap_T;	 // wrapping mode on T axis
	unsigned int Filter_Min; // filtering mode if texture pixels < screen pixels
	unsigned int Filter_Max; // filtering mode if texture pixels > screen pixels

	// constructor (sets default texture modes)
	Texture2D();
	Texture2D(unsigned int width, unsigned int height);
	Texture2D(unsigned int width, unsigned int height, const void* const data);

	Texture2D(const Texture2D&) = delete;
	Texture2D(Texture2D&&) noexcept;
	~Texture2D() noexcept;

	// generates texture from image data
	void Generate(unsigned int width, unsigned int height, const void* const data);

	const Texture2D& Active(unsigned samplerID) const noexcept;
	const Texture2D& Bind() const;

	bool Render(unsigned int fbo);

	bool Validate() const noexcept;
};
