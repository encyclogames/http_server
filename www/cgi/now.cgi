#!/usr/bin/env python3

import time
print("Content-Type: text/html\r\n")  # html markup follows
print("Content-Length: 150\r\n\r\n")

timeStr = time.strftime("%c") # obtains current time

htmlFormat = """
<html>
  <Title>The Time Now</Title>
<body>
  <p>The current Central date and time is:  {timeStr}</p>
</body>
</html> """

print(htmlFormat.format(**locals())) # see embedded %s ^ above
