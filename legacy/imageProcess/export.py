from svg.path import parse_path
from xml.dom.minidom import parse

def print_bezier_info(path_str, file):
    path = parse_path(path_str)
    # i = 0
    for segment in path:
        # i+=1
        # print(i)
        segment_type = type(segment).__name__
        if segment_type == 'CubicBezier':
            # print(f"(1-t)(1-t)(1-t)({int(segment.start.real/20)},  {int(segment.start.imag/20)})+3(1-t)(1-t)t({int(segment.control1.real/20)}, {int(segment.control1.imag/20)})+3(1-t)tt({int(segment.control2.real/20)}, {int(segment.control2.imag/20)})+ttt({int(segment.end.real/20)}, {int(segment.end.imag/20)})\n")
            file.write(f"CubicBezier {int(segment.start.real)} {int(segment.start.imag)} {int(segment.control1.real)} {int(segment.control1.imag)} {int(segment.control2.real)} {int(segment.control2.imag)} {int(segment.end.real)} {int(segment.end.imag)}\n")
        elif segment_type == 'QuadraticBezier':
            # print(f"CubicBezier {int(segment.start.real)} {int(segment.start.imag)} {int(segment.control1.real)} {int(segment.control1.imag)} {int(segment.control2.real)} {int(segment.control2.imag)} {int(segment.end.real)} {int(segment.end.imag)}\n")
            file.write(f"QuadraticBezier {int(segment.start.real)} {int(segment.start.imag)} {int(segment.control1.real)} {int(segment.control1.imag)} 0 0 {int(segment.end.real)} {int(segment.end.imag)}\n")
        elif segment_type == 'Line':
            # print(f"CubicBezier {int(segment.start.real)} {int(segment.start.imag)} {int(segment.end.real)} {int(segment.end.imag)}\n")
            file.write(f"Line {int(segment.start.real)} {int(segment.start.imag)} 0 0 0 0 {int(segment.end.real)} {int(segment.end.imag)}\n")


for i in range(1, 2):
    # svg_file_path = r'D:\img\svg\frame' + str(i) + ".svg"
    # output_file_path = r'D:\img\data\frame' + str(i) + ".txt"
    svg_file_path = r"C:\Users\sakam\Downloads\1.svg"
    output_file_path = r"bruh.txt"
    # print(svg_file_path)
    # print(output_file_path)

    with open(output_file_path, 'w') as output_file:
        dom = parse(svg_file_path)
        paths = dom.getElementsByTagName("path")
        i = 0
        for path in paths:
            for path_d_attribute in parse_path(path.getAttribute('d')):
                i += 1
        output_file.write(str(i) + '\n')
        i = 0
        for path in paths:
            path_d_attribute = path.getAttribute('d')
            print_bezier_info(path_d_attribute, output_file)
