import os

class TextStyle(object):

	styles = {
		'default': '\033[m',

		'bold': '\033[1m',
		'underline': '\033[4m',
		'blink': '\033[5m',
		'reverse': '\033[7m',
		'concealed': '\033[8m',

		'black': '\033[30m',
		'red': '\033[31m',
		'green': '\033[32m',
		'yellow': '\033[33m',
		'blue': '\033[34m',
		'magenta': '\033[35m',
		'cyan': '\033[36m',
		'white': '\033[37m',

		'bg_black': '\033[40m',
		'bg_red': '\033[41m',
		'bg_green': '\033[42m',
		'bg_yellow': '\033[43m',
		'bg_blue': '\033[44m',
		'bg_magenta': '\033[45m',
		'bg_cyan': '\033[46m',
		'bg_white': '\033[47m'
	}

	@staticmethod
	def style(text, style):
		if not os.isatty(1):
			return text
		if style not in TextStyle.styles:
			print 'text style does not exist'
		return TextStyle.styles[style] + text + TextStyle.styles['default']
		