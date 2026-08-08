import os
import cv2
import matplotlib.pyplot as plt

folder_path = r'D:\img\frame'

files = [f for f in os.listdir(folder_path) if os.path.isfile(os.path.join(folder_path, f))]

images = []
i = 0
for file in files:
    # print(file)
    i+=1
    image_path = os.path.join(folder_path, file)
    image = cv2.imread(image_path, 0)
    if image is not None:
        images.append(image)
        _, cannyImage = cv2.threshold(image, 0, 255, cv2.THRESH_OTSU)
        # inverted_edges = cv2.bitwise_not(cannyImage)
        output_image_path = os.path.join(r"D:\img\canny", f'frame{i}.png')
        cv2.imwrite(output_image_path, cannyImage)
    else:
        print(f'Không thể đọc ảnh: {image_path}')

# img = r'D:\img\frame\frame15.png'
# image = cv2.imread(img, 0)
# cannyImage = cv2.Canny(image, 0, 255)
# plt.imshow(cannyImage, "gray")
# plt.waitforbuttonpress()
# plt.close("all")