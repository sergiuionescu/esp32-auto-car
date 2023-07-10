import re

index_html = 'index.html'
ino_file = 'esp32.ino'

with open(index_html, 'r') as source:
    index_html_text = source.read().replace('\n', '').replace('\r', '')


with open(ino_file, 'r') as target:
    ino_file_text = target.read()

with open(ino_file, 'w') as target:
    patched_text = re.sub('<!--START:INDEX-->.*<!--END:INDEX-->', index_html_text, ino_file_text)
    target.write(patched_text)


print('Done')
