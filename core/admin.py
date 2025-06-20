from django.contrib import admin
from core.models import Driver, VerificationRecord


@admin.register(Driver)
class DriverAdmin(admin.ModelAdmin):
	list_per_page = 25
	list_display = ["id", "national_id", "first_name", "last_name", "barcode", "is_permit_valid", "valid_up_to"]

	def is_permit_valid(self, obj):
		return obj.is_permit_valid()  # Call the method from the model

	is_permit_valid.boolean = True


@admin.register(VerificationRecord)
class VerificationRecordAdmin(admin.ModelAdmin):
	list_per_page = 25
	list_display = ["driver", 'is_permit_valid', 'valid_up_to', "is_manual"]

	def is_permit_valid(self, scan):
		return scan.driver.is_permit_valid()

	is_permit_valid.boolean = True

	def valid_up_to(self, scan):
		return scan.driver.valid_up_to


admin.site.site_header = 'Police: Permit monitoring system'
admin.site.site_title = 'Drivers permit system'
admin.site.index_title = 'Permit monitoring Administration'
