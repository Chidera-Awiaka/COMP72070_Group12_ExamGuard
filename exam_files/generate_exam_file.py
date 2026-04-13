from reportlab.lib.pagesizes import letter

from reportlab.pdfgen import canvas


c = canvas.Canvas("exam.pdf", pagesize=letter)


text = c.beginText(40, 750)


# Repeat content to make file large (~1MB)

for i in range(5000):

    text.textLine(f"Exam Question {i+1}: Explain something important.")


c.drawText(text)

c.save()
