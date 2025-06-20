from django.db import models
from django.utils import timezone


class Driver(models.Model):
	"""
	Models a driver. Each driver has a dedicated barcode that uniquely identifies him/her.
	"""
	first_name = models.CharField(max_length=50)
	last_name = models.CharField(max_length=50)
	gender = models.IntegerField(choices=((1, "Male"), (2, "Female")))
	region = models.IntegerField(choices=((1, "Manzini"), (2, "Hhohho"), (3, "Shiselweni"), (4, "Lubombo")), default=1)

	national_id = models.CharField(max_length=50)
	licence_number = models.CharField(max_length=50)

	barcode = models.CharField(max_length=100)
	valid_up_to = models.DateTimeField()

	phone_number = models.CharField(max_length=50, null=True, blank=True)

	def full_name(self):
		return f"{self.first_name} {self.last_name}"

	def __str__(self):
		return self.full_name()

	def format_date(self):
		return self.valid_up_to.strftime("%m/%y")

	def get_initials(self):
		first_initial = self.first_name[0].upper() if self.first_name else ""
		return f"{first_initial}. {self.last_name[:9]}"

	def is_permit_valid(self):
		return self.valid_up_to >= timezone.now()

	def censored_id(self):
		return f"********{self.national_id[8:]}"


class VerificationRecord(models.Model):
	driver = models.ForeignKey(Driver, related_name="records", on_delete=models.CASCADE)
	is_manual = models.BooleanField(default=False)
	created_at = models.DateTimeField(auto_now_add=True)
