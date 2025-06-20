from django.urls import path
from api.views import (
	barcode_verification, load_latest_verification_records
)

app_name = "api"
urlpatterns = [
	path('verify/', barcode_verification),
	path('load-latest-records/', load_latest_verification_records),
]
