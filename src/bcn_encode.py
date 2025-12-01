import PIL
import io

buffer = io.BytesIO ()
PIL.ImageFile._save (image, buffer, [PIL.ImageFile._Tile ("bcn", (0, 0) + (width, height), 0, (n, ))])
result = buffer.getvalue ()