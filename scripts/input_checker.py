import argparse
from collections import Counter

parser = argparse.ArgumentParser(description="Checks for Hanstholm input file for common problems.")
parser.add_argument('input')
args = parser.parse_args()

file_stats = {}
counts = Counter()


class SentenceChecker():
	def __init__(self):
		self.counts = Counter()
		pass

	def check(self, lines, line_no):
		self.line_no = line_no
		self._parse_heads_and_labels(lines)
		self._check_ranges()
		self.counts['num_nonprojective_edges'] += self._num_nonprojective_edges()
		self.counts['num_edges'] += len(self.heads)

	def _check_ranges(self):
		# Range check
		for i, head in enumerate(self.heads):
			if head < -1 or head >= len(self.heads):
				self.add_issue("Head {} for token {} is outside the allowed range (-1, {})".format(head, i, len(self.heads) - 1), 
					self.ids[i])
				# . Hanstholm assumes 0-indexed tokens. Root node is -1
				# is outside sentence. 

	def _parse_heads_and_labels(self, lines):
		self.labels = []
		self.heads = []
		self.ids = []
		for line in lines:
			parts = line.split(" ")
			joint_label = parts[0]
			label_divider = joint_label.rfind("-")
	
			self.heads.append(int(joint_label[:label_divider]))
			self.labels.append(joint_label[label_divider+1:])
			
			id_and_ns = parts[1].split("|")

			self.ids.append(id_and_ns[0][1:])

	def _num_nonprojective_edges(self):
		non_projective = 0

		# For all edges (u, v) in sentence
		for u, v in enumerate(self.heads):
			# Calculate inclusive and exclusive spans
			inner_span = range(min(u, v) + 1, max(u, v) - 1)
			span = range(min(u, v), max(u, v) + 1)		

			# An edge (u, v) is non-projective
			# if there's another edge (s, t) from inside the span to outside the span
			for s, t in enumerate(self.heads):
				node_within_span = (s in inner_span or t in inner_span)
				node_outside_span = not (s in span and t in span)

				if node_within_span and node_outside_span:
					non_projective += 1

		return non_projective

	def add_issue(self, message, token_id):
		print("LINE ", line_no, end=' ')
		if token_id:
			print("[id={}]".format(token_id), end=' ')
		print(":", end=' ')
		print(message)






checker = SentenceChecker()
sent = []
sent_start_line_no = 1

for line_no, line in enumerate(open(args.input)):
	line = line.strip()
	if (len(line) == 0):
		checker.check(sent, sent_start_line_no)
		sent = []
		sent_start_line_no = line_no + 1
	else:
		sent.append(line)

if (len(sent)):
	checker.check(sent, sent_start_line_no)

print(checker.counts)