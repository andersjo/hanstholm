import argparse
import codecs

parser = argparse.ArgumentParser(description="Checks for Hanstholm input file for common problems.")
parser.add_argument('input')
args = parser.parse_args()

heads = []
def add_issue(message):
	print message

def is_projective():
	pass

def check_sent(lines):
	global heads
	heads = []

	labels = []
	for line in lines:
		parts = line.split(" ")
		joint_label = parts[0]
		print parts
		label_divider = joint_label.rfind("-")

		heads.append(int(joint_label[:label_divider]))
		labels.append(joint_label[label_divider+1:])
	
	# Range check
	for i, head in enumerate(heads):
		if head < -1 or head >= len(heads):
			add_issue("Head index is outside sentence. Hanstholm assumes 0-indexed tokens. Root node is -1")

	# Projectivity
	for u, v in enumerate(heads):
		inner_span = range(min(u, v) + 1, max(u, v) - 1)
		span = range(min(u, v), max(u, v) + 1)		

		for s, t in enumerate(heads):
			node_within_span = (s in inner_span or t in inner_span)
			node_outside_span = not (s in span and t in span)

			if node_within_span and node_outside_span:
				add_issue("Sentence is not projective. Edge between {}-{} crosses {}-{}".format(head_j, j, head_i, i))




sent = []
for line in codecs.open(args.input, encoding='utf-8'):
	line = line.strip()
	if (len(line) == 0):
		check_sent(sent)
		sent = []

	sent.append(line)

if (len(sent)):
	check_sent(sent)
